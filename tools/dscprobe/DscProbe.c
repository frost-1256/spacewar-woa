/* Fixed-offset, one-shot v3.8 read-only probe; no exported interface.
 * Display must be powered and clocked by the normal display driver.
 * Never map, power up, acknowledge, clear, or write hardware here.
 */
#include <ntddk.h>
#include <aux_klib.h>
#include <stdarg.h>

#if !defined(_M_ARM64)
#error Build in an ARM64-targeting EWDK shell.
#endif

#define TAG 'PcSD'
#define REPORT_SIZE 16384
static CHAR Report[REPORT_SIZE];
static SIZE_T Used;
static PCSTR Error;
static PCSTR Stage = "init";
static KEVENT Finished;
static VOID SaveReport(BOOLEAN final);
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD ProbeUnload;
IO_WORKITEM_ROUTINE_EX ProbeWorker;

static VOID Fail(PCSTR stage)
{
    if (Error == NULL) Error = stage;
}

/* Hand-rolled formatter: the WDK kernel libs do not provide the UCRT
 * __stdio_common_vsprintf that RtlStringCbVPrintfA needs. Supports exactly
 * the conversions used below: %s, %u, %x with optional 0-width, %llx. */
static VOID AppendRaw(PCSTR s, SIZE_T n)
{
    SIZE_T i;
    for (i = 0; i < n; ++i) {
        if (Used >= sizeof(Report) - 1) { Fail("report_capacity"); return; }
        Report[Used++] = s[i];
    }
    Report[Used] = '\0';
}

static VOID AppendStr(PCSTR s)
{
    SIZE_T n = 0;
    if (s == NULL) s = "(null)";
    while (s[n]) ++n;
    AppendRaw(s, n);
}

static VOID AppendHex(ULONGLONG v, ULONG minWidth)
{
    CHAR buf[20];
    ULONG n = 0, i;
    if (v == 0) buf[n++] = '0';
    while (v) {
        ULONG d = (ULONG)(v & 0xF);
        buf[n++] = (CHAR)(d < 10 ? '0' + d : 'a' + (d - 10));
        v >>= 4;
    }
    while (n < minWidth) buf[n++] = '0';
    for (i = 0; i < n / 2; ++i) { CHAR t = buf[i]; buf[i] = buf[n - 1 - i]; buf[n - 1 - i] = t; }
    AppendRaw(buf, n);
}

static VOID AppendDec(ULONG v)
{
    CHAR buf[12];
    ULONG n = 0, i;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = (CHAR)('0' + (v % 10)); v /= 10; }
    for (i = 0; i < n / 2; ++i) { CHAR t = buf[i]; buf[i] = buf[n - 1 - i]; buf[n - 1 - i] = t; }
    AppendRaw(buf, n);
}

static VOID Line(PCSTR format, ...)
{
    va_list args;
    PCSTR f;
    va_start(args, format);
    for (f = format; *f;) {
        ULONG width = 0;
        BOOLEAN lng = FALSE;
        if (*f != '%') { AppendRaw(f, 1); ++f; continue; }
        ++f;
        if (*f == '%') { AppendRaw(f, 1); ++f; continue; }
        if (*f == '0') {
            ++f;
            while (*f >= '0' && *f <= '9') { width = width * 10 + (ULONG)(*f - '0'); ++f; }
        }
        if (f[0] == 'l' && f[1] == 'l') { lng = TRUE; f += 2; }
        switch (*f) {
        case 's': AppendStr(va_arg(args, PCSTR)); break;
        case 'u': AppendDec(va_arg(args, ULONG)); break;
        case 'x':
            if (lng) AppendHex(va_arg(args, ULONGLONG), width);
            else AppendHex(va_arg(args, ULONG), width);
            break;
        default: break;
        }
        if (*f) ++f;
    }
    va_end(args);
}

static ULONG_PTR At(ULONG_PTR base, SIZE_T offset)
{
    return base && offset <= MAXULONG_PTR - base ? base + offset : 0;
}

static BOOLEAN KernelRange(ULONG_PTR p, SIZE_T size)
{
    return p != 0 && p >= (ULONG_PTR)MmSystemRangeStart && size != 0 &&
        size - 1 <= MAXULONG_PTR - p;
}

/* Inspect every byte, including both ends of page-crossing ranges. The checks
 * are not a lifetime lock: SEH also surrounds checks and the subsequent read. */
static BOOLEAN Read(ULONG_PTR p, PVOID out, SIZE_T size, BOOLEAN mmio)
{
    SIZE_T i;
    if (!KernelRange(p, size)) return FALSE;
    __try {
        for (i = 0; i < size; ++i)
            if (!MmIsAddressValid((PVOID)(p + i))) return FALSE;
        if (mmio) {
            if (size != sizeof(ULONG) || (p & 3)) return FALSE;
            *(PULONG)out = READ_REGISTER_ULONG((PULONG)p);
        } else {
            for (i = 0; i < size; ++i)
                ((PUCHAR)out)[i] = ((volatile UCHAR *)p)[i];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN Word(PCSTR key, ULONG_PTR p, PULONG value, PCSTR stage)
{
    if (Read(p, value, sizeof(*value), FALSE)) {
        Line("%s=0x%08x\r\n", key, *value);
        return TRUE;
    }
    Line("%s=unavailable\r\n", key);
    Fail(stage);
    return FALSE;
}

static BOOLEAN FindImage(ULONG_PTR *base, PULONG imageSize)
{
    PAUX_MODULE_EXTENDED_INFO modules = NULL;
    ULONG bytes = 0, capacity = 0, count, i, attempt;
    NTSTATUS s = AuxKlibInitialize();
    BOOLEAN found = FALSE;
    if (!NT_SUCCESS(s)) { Fail("aux_init"); return FALSE; }
    /* A module may load between sizing and querying; bounded retries only. */
    for (attempt = 0; attempt < 3; ++attempt) {
        s = AuxKlibQueryModuleInformation(&bytes, sizeof(*modules), NULL);
        if (!NT_SUCCESS(s) || bytes == 0) break;
        capacity = bytes;
        modules = ExAllocatePool2(POOL_FLAG_PAGED, capacity, TAG);
        if (modules == NULL) { Fail("module_alloc"); return FALSE; }
        s = AuxKlibQueryModuleInformation(&bytes, sizeof(*modules), modules);
        if (NT_SUCCESS(s) && bytes <= capacity) break;
        ExFreePoolWithTag(modules, TAG);
        modules = NULL;
        if (s != STATUS_BUFFER_TOO_SMALL) break;
    }
    if (modules == NULL || !NT_SUCCESS(s) || bytes > capacity) {
        if (modules) ExFreePoolWithTag(modules, TAG);
        Fail("module_query");
        return FALSE;
    }
    count = bytes / sizeof(*modules);
    for (i = 0; i < count; ++i) {
        ANSI_STRING name, wanted;
        USHORT offset = modules[i].FileNameOffset;
        SIZE_T n, limit = sizeof(modules[i].FullPathName);
        if (offset >= limit) continue;
        for (n = offset; n < limit && modules[i].FullPathName[n]; ++n) { }
        if (n == limit) continue;
        name.Buffer = (PCHAR)modules[i].FullPathName + offset;
        name.Length = (USHORT)(n - offset);
        name.MaximumLength = name.Length;
        RtlInitAnsiString(&wanted, "qcdxkm7325.sys");
        if (RtlEqualString(&name, &wanted, TRUE)) {
            *base = (ULONG_PTR)modules[i].BasicInfo.ImageBase;
            *imageSize = modules[i].ImageSize;
            found = TRUE;
            break;
        }
    }
    ExFreePoolWithTag(modules, TAG);
    if (!found) { Line("qcdxkm=not_loaded\r\n"); Fail("module_not_loaded"); }
    return found;
}

static BOOLEAN Quad(PCSTR key, ULONG_PTR p, ULONGLONG *value)
{
    if (Read(p, value, sizeof(*value), FALSE)) {
        Line("%s=0x%llx\r\n", key, *value);
        return TRUE;
    }
    Line("%s=unavailable\r\n", key);
    Fail("gx3_guard");
    return FALSE;
}

static VOID ProbeGx3(ULONG_PTR base, ULONG_PTR mdss, ULONG_PTR *intf)
{
    ULONGLONG g = 0, t3c0 = 0, t3c8 = 0;
    ULONGLONG x15 = 0, encs = 0, ctls = 0, ctloff = 0, intfoff = 0;
    ULONG panel[2][5] = {{0}}, mask = 0, idx = 0, dce = 0, var = 0;
    ULONG n, i, j, selected, value;
    ULONG_PTR pi, dsc, regs[4], p;
    BOOLEAN active[2], ok = FALSE;
    static const ULONG panelOffsets[] = {0x1E40,0x1E48,0x1E58,0x1E5C,0x1A18};
    static const PCSTR panelKeys[] = {"paths", "ctl", "intf", "raw", "dscon"};
    static const ULONG encOffsets[] = {0x000,0x100,0x130,0x134,0x138,0x13C,0x140,0x164};
    static const ULONG dscctlOffsets[] = {0xF00,0xF04,0xF08};
    static const ULONG ctlOffsets[] = {0xE8,0x104,0xF4,0xE4,0x14,0x18};
    static const ULONG intfOffsets[] = {0x25C};
    static const ULONG * const regOffsets[] = {encOffsets, dscctlOffsets, ctlOffsets, intfOffsets};
    static const ULONG regCounts[] = {
        RTL_NUMBER_OF(encOffsets), RTL_NUMBER_OF(dscctlOffsets),
        RTL_NUMBER_OF(ctlOffsets), RTL_NUMBER_OF(intfOffsets)
    };
    static const PCSTR regKeys[] = {"enc", "dscctl", "ctl", "intf"};

    *intf = 0;
    Stage = "gx3";
    SaveReport(FALSE);
    if (!Quad("gx3_g", At(base, 0x2274D0), &g) ||
        !KernelRange((ULONG_PTR)g, 0x3D0) ||
        !Word("gx3_mask", At((ULONG_PTR)g, 0x80), &mask, "gx3_guard") ||
        !Quad("gx3_t3c0", At((ULONG_PTR)g, 0x3C0), &t3c0) ||
        !Quad("gx3_t3c8", At((ULONG_PTR)g, 0x3C8), &t3c8)) goto done;
    if (!KernelRange((ULONG_PTR)t3c0, 8 * sizeof(ULONG)) ||
        !KernelRange((ULONG_PTR)t3c8, 8 * sizeof(ULONG))) goto done;
    for (n = 0; n < 2; ++n) {
        pi = At(base, 0x2863A0 + n * 0x2A10);
        for (i = 0; i < RTL_NUMBER_OF(panelOffsets); ++i) {
            if (!Read(At(pi, panelOffsets[i]), &panel[n][i], sizeof(ULONG), FALSE)) {
                Line("%s_%u=unavailable\r\n", panelKeys[i], n);
                goto done;
            }
            Line("%s_%u=0x%08x\r\n", panelKeys[i], n, panel[n][i]);
        }
        active[n] = panel[n][0] >= 1 && panel[n][4] == 1;
    }
    selected = active[0] != active[1] ? (active[0] ? 0 : 1) : 1;
    if (active[0] == active[1]) Line("pathsel=fallback1\r\n");
    else Line("pathsel=n%u\r\n", selected);
    if (panel[selected][3] >= 16 || panel[selected][1] >= 8 ||
        panel[selected][2] >= 8) goto done;
    if (!Word("gx3_idx", At(base, 0x1C73C0 + 4 * panel[selected][3]), &idx, "gx3_guard") ||
        idx >= 8) goto done;
    if (!Word("gx3_dce", At((ULONG_PTR)t3c8, 4 * idx), &dce, "gx3_guard") ||
        dce >= 3) goto done;
    if (!Quad("gx3_x15", At(base, 0x285220 + 8 * dce), &x15) ||
        !Quad("gx3_encs", At(base, 0x285280 + 8 * idx), &encs) ||
        !Quad("gx3_ctls", At(base, 0x285240 + 8 * idx), &ctls) ||
        !Word("gx3_var", At((ULONG_PTR)t3c0, 4 * idx), &var, "gx3_guard") ||
        !Quad("gx3_ctloff", At(base, 0x2853E0 + 8 * panel[selected][1]), &ctloff) ||
        !Quad("gx3_intfoff", At(base, 0x2852E0 + 8 * panel[selected][2]), &intfoff)) goto done;
    if (x15 > 0x10000 || encs > 0x10000 || ctls > 0x10000 ||
        ctloff > 0x10000 || intfoff > 0x10000 ||
        (!(mask & (1u << idx)) && idx != 1)) goto done;
    dsc = At(mdss, (SIZE_T)x15 + 0x81000);
    regs[0] = At(dsc, (SIZE_T)encs);
    regs[1] = At(dsc, (SIZE_T)ctls);
    regs[2] = At(mdss, (SIZE_T)ctloff + 0x16000);
    regs[3] = At(mdss, (SIZE_T)intfoff + 0x35000);
    Line("gx3_dsc_va=0x%llx\r\ngx3_ctl_va=0x%llx\r\ngx3_intf_va=0x%llx\r\n",
        (ULONGLONG)dsc, (ULONGLONG)regs[2], (ULONGLONG)regs[3]);
    /* Validate every register address before the first GX-R3 MMIO read. */
    for (i = 0; i < RTL_NUMBER_OF(regs); ++i) {
        for (j = 0; j < regCounts[i]; ++j) {
            p = At(regs[i], regOffsets[i][j]);
            if (!KernelRange(p, sizeof(ULONG)) || (p & 3) ||
                p < mdss || p - mdss >= 0x95000) goto done;
        }
    }
    *intf = regs[3];
    for (i = 0; i < RTL_NUMBER_OF(regs); ++i) {
        for (j = 0; j < regCounts[i]; ++j) {
            if (!Read(At(regs[i], regOffsets[i][j]), &value, sizeof(value), TRUE)) {
                Line("%s[0x%03x]=unavailable\r\n", regKeys[i], regOffsets[i][j]);
                goto done;
            }
            Line("%s[0x%03x]=0x%08x\r\n", regKeys[i], regOffsets[i][j], value);
        }
    }
    ok = TRUE;
done:
    if (!ok) Fail("gx3_guard");
    Line("gx3=%s\r\n", ok ? "ok" : "aborted_guard");
    Stage = "gx3_done";
    SaveReport(FALSE);
}


/* Preflight each complete group before reading any member. Pointer-dependent
 * groups are preflighted after their parent pointer has been read. Read repeats
 * these per-byte checks under SEH at the actual dereference (no lifetime lock). */
static BOOLEAN R34Mapped(ULONG_PTR p, SIZE_T size, BOOLEAN mmio)
{
    SIZE_T i;
    if (!KernelRange(p, size) || (mmio && (size != sizeof(ULONG) || (p & 3))))
        return FALSE;
    __try {
        for (i = 0; i < size; ++i)
            if (!MmIsAddressValid((PVOID)(p + i))) return FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}

static BOOLEAN R34Word(PCSTR key, ULONG_PTR p, BOOLEAN ready,
    BOOLEAN mmio, PULONG value)
{
    if (ready && Read(p, value, sizeof(*value), mmio)) {
        Line("%s=0x%08x\r\n", key, *value);
        return TRUE;
    }
    Line("%s=unavailable\r\n", key);
    Fail("r34_guard");
    return FALSE;
}

static VOID ProbeR34(ULONG_PTR base, ULONG disp, BOOLEAN objectOk,
    ULONG_PTR mdss, ULONG_PTR intf)
{
    static const ULONG piOffsets[] = {
        0x24,0x1A08,0x4AC,0x1CC8,
        0x2748+0x28,0x2748+0x2C,0x2748+0x34,
        0x2748+0x38,0x2748+0x3C,0x2748+0x40
    };
    static const PCSTR piKeys[] = {
        "r34_pi24","r34_pi1a08","r34_pi4ac","r34_pi1cc8",
        "r34_out28","r34_out2c","r34_out34",
        "r34_out38","r34_out3c","r34_out40"
    };
    static const ULONG calcOffsets[] = {0x34,0x38,0x40};
    static const PCSTR calcKeys[] = {"r34_calc34","r34_calc38","r34_calc40"};
    static const ULONG mmioOffsets[] = {0x040,0x044,0x1B8,0x060,0x064};
    static const PCSTR mmioKeys[] = {
        "r34_dsi040","r34_dsi044","r34_dsi1b8","r34_intf060","r34_intf064"
    };
    ULONG_PTR pi = 0, calcSlot, tableSlot, desc = 0, dsi, canary;
    ULONG_PTR calcVa[3], mmioVa[5];
    ULONGLONG calculator = 0, table = 0, base2 = 0;
    ULONG values[10] = {0}, value = 0, i;
    BOOLEAN piReady[10], piRead[10], mmioReady[5];
    BOOLEAN piRange, calcSlotReady, tableSlotReady, canaryReady;
    BOOLEAN calcOk, descOk = FALSE, canaryOk, ok = TRUE;

    Stage = "r34";
    SaveReport(FALSE);
    /* disp is the slot whose controller validated, never gx3's path selector. */
    if (objectOk && disp < 4) pi = At(base, 0x2863A0 + (SIZE_T)disp * 0x2A10);
    piRange = KernelRange(pi, 0x27B8);
    calcSlot = At(pi, 0x27B0);
    tableSlot = At(base, 0x285928);
    dsi = At(mdss, 0x94000);
    canary = At(dsi, 0x58);
    /* All statically resolvable VAs are checked before the first stage read. */
    for (i = 0; i < RTL_NUMBER_OF(piOffsets); ++i)
        piReady[i] = piRange && R34Mapped(At(pi, piOffsets[i]), sizeof(ULONG), FALSE);
    calcSlotReady = piRange && R34Mapped(calcSlot, sizeof(calculator), FALSE);
    tableSlotReady = R34Mapped(tableSlot, sizeof(table), FALSE);
    canaryReady = KernelRange(mdss, 0x95000) && R34Mapped(canary, sizeof(ULONG), TRUE);
    for (i = 0; i < RTL_NUMBER_OF(mmioVa); ++i) {
        mmioVa[i] = At(i < 3 ? dsi : intf, mmioOffsets[i]);
        /* INTF_0 clock state is not established; never sample it here. */
        mmioReady[i] = (i < 3 || intf != At(mdss, 0x35000)) &&
            KernelRange(mdss, 0x95000) &&
            mmioVa[i] >= mdss && mmioVa[i] - mdss <= 0x95000 - sizeof(ULONG) &&
            R34Mapped(mmioVa[i], sizeof(ULONG), TRUE);
    }
    for (i = 0; i < RTL_NUMBER_OF(piOffsets); ++i) {
        piRead[i] = R34Word(piKeys[i], At(pi, piOffsets[i]), piReady[i], FALSE, &values[i]);
        if (!piRead[i]) ok = FALSE;
    }
    calcOk = calcSlotReady && Read(calcSlot, &calculator, sizeof(calculator), FALSE) &&
        KernelRange((ULONG_PTR)calculator, 0x44);
    for (i = 0; i < RTL_NUMBER_OF(calcVa); ++i) {
        calcVa[i] = At((ULONG_PTR)calculator, calcOffsets[i]);
        if (!R34Mapped(calcVa[i], sizeof(ULONG), FALSE)) calcOk = FALSE;
    }
    Line("r34_calc_ok=%u\r\n", (ULONG)calcOk);
    if (!calcOk) { Fail("r34_guard"); ok = FALSE; }
    for (i = 0; i < RTL_NUMBER_OF(calcVa); ++i) {
        BOOLEAN readOk = R34Word(calcKeys[i], calcVa[i], calcOk, FALSE, &value);
        if (!readOk) ok = FALSE;
    }

    if (piRead[3] && (values[3] & (1u << 20))) {
        Line("r34_desc_gate=alt\r\n");
    } else {
        if (piRead[3] && piRead[0] && tableSlotReady &&
            Read(tableSlot, &table, sizeof(table), FALSE) &&
            KernelRange((ULONG_PTR)table, 16) &&
            R34Mapped(At((ULONG_PTR)table, 8), sizeof(base2), FALSE) &&
            Read(At((ULONG_PTR)table, 8), &base2, sizeof(base2), FALSE) &&
            KernelRange((ULONG_PTR)base2, 1)) {
            /* Promote type before multiplication; At rejects address overflow. */
            desc = At((ULONG_PTR)base2, (SIZE_T)2 * values[0] * 16);
            descOk = KernelRange(desc, 8);
            if (!R34Mapped(desc, sizeof(ULONG), FALSE)) descOk = FALSE;
            if (!R34Mapped(At(desc, 4), sizeof(ULONG), FALSE)) descOk = FALSE;
        }
        Line("r34_desc_gate=%s\r\n", descOk ? "walked" : "unavailable");
        if (!R34Word("r34_desc0", desc, descOk, FALSE, &value)) ok = FALSE;
        if (!R34Word("r34_desc4", At(desc, 4), descOk, FALSE, &value)) ok = FALSE;
    }
    /* Fresh guarded read, without a DSC-on value fingerprint: DSC-off differs.
     * Display-up is an operator precondition, not inferred from register data. */
    canaryOk = canaryReady && Read(canary, &value, sizeof(value), TRUE);
    for (i = 0; i < RTL_NUMBER_OF(mmioVa); ++i) {
        BOOLEAN readOk = R34Word(mmioKeys[i], mmioVa[i], canaryOk && mmioReady[i], TRUE, &value);
        if (!readOk) ok = FALSE;
    }
    Line("r34=%s\r\n", ok ? "ok" : "aborted_guard");
    Stage = "r34_done";
    SaveReport(FALSE);
}

static BOOLEAN V38Word(PCSTR key, ULONG_PTR p, BOOLEAN ready,
    BOOLEAN mmio, PULONG value)
{
    if (ready && Read(p, value, sizeof(*value), mmio)) {
        Line("%s=0x%08x\r\n", key, *value);
        return TRUE;
    }
    Line("%s=unavailable\r\n", key);
    Fail("v38_guard");
    return FALSE;
}

static BOOLEAN V38MmioReady(ULONG_PTR mdss, ULONG_PTR p)
{
    return KernelRange(mdss, 0x95000) && p >= mdss &&
        p - mdss <= 0x95000 - sizeof(ULONG) &&
        R34Mapped(p, sizeof(ULONG), TRUE);
}

static VOID ProbeV38Panel(ULONG_PTR base, ULONG imageSize, ULONG disp,
    BOOLEAN objectOk)
{
    static const ULONG offsets[] = {
        0x1A18,0x1A1C,0x1CC,0x279C,0x27AC,0x4BC,
        0x1A20,0x1A24,0x1A28,0x1A2C,0x1A30,0x1A34,0x1A38,0x1A3C
    };
    static const PCSTR keys[] = {
        "v38_pi_dsc_enable","v38_pi_mode_index","v38_pi_bit_clock_override",
        "v38_pi_bytes_in_slice","v38_pi_slice_count","v38_pi_auto_pps_flags",
        "v38_pi_dsc_raw[0x00]","v38_pi_dsc_raw[0x04]",
        "v38_pi_dsc_raw[0x08]","v38_pi_dsc_raw[0x0c]",
        "v38_pi_dsc_raw[0x10]","v38_pi_dsc_raw[0x14]",
        "v38_pi_dsc_raw[0x18]","v38_pi_dsc_raw[0x1c]"
    };
    ULONG_PTR pi = 0;
    SIZE_T rva = 0;
    ULONG i, value;
    BOOLEAN ready[RTL_NUMBER_OF(offsets)], range = FALSE, ok = TRUE;
    Stage = "v38_panel";
    SaveReport(FALSE);
    if (objectOk && disp < 4) {
        rva = 0x2863A0 + (SIZE_T)disp * 0x2A10;
        pi = At(base, rva);
        range = rva <= imageSize && 0x27B0 <= imageSize - rva &&
            KernelRange(pi, 0x27B0);
    }
    Line("v38_pi_disp=%u\r\nv38_pi_va=0x%llx\r\n", disp, (ULONGLONG)pi);
    for (i = 0; i < RTL_NUMBER_OF(offsets); ++i)
        ready[i] = range && R34Mapped(At(pi, offsets[i]), sizeof(ULONG), FALSE);
    for (i = 0; i < RTL_NUMBER_OF(offsets); ++i)
        if (!V38Word(keys[i], At(pi, offsets[i]), ready[i], FALSE, &value)) ok = FALSE;
    Line("v38_panel=%s\r\n", ok ? "ok" : "aborted_guard");
    Stage = "v38_panel_done";
    SaveReport(FALSE);
}

static VOID ProbeV38Dsi(ULONG_PTR mdss, BOOLEAN mmioGate)
{
    /* STREAM0_CTRL/TOTAL, COMPRESSION_MODE_CTRL/CTRL2/CTRL3 are already
     * mmio[0x058/05c/2a8/2ac/2b0]; CMD_MODE_MDP_CTRL2 is r34_dsi1b8.
     * Error values are residual snapshots: the display driver may clear W1C. */
    static const ULONG offsets[] = {0x00C,0x068,0x0B4,0x0C0,0x10C,0x110,0x120};
    static const PCSTR keys[] = {
        "v38_dsi_fifo_status","v38_dsi_ack_err_status","v38_dsi_phy_err",
        "v38_dsi_timeout","v38_dsi_err_int_mask","v38_dsi_int_ctrl",
        "v38_dsi_clk_status"
    };
    BOOLEAN ready[RTL_NUMBER_OF(offsets)], ok = TRUE;
    ULONG_PTR dsi = At(mdss, 0x94000);
    ULONG i, value;
    Stage = "v38_dsi";
    SaveReport(FALSE);
    for (i = 0; i < RTL_NUMBER_OF(offsets); ++i)
        ready[i] = mmioGate && V38MmioReady(mdss, At(dsi, offsets[i]));
    for (i = 0; i < RTL_NUMBER_OF(offsets); ++i)
        if (!V38Word(keys[i], At(dsi, offsets[i]), ready[i], TRUE, &value)) ok = FALSE;
    Line("v38_dsi=%s\r\n", ok ? "ok" : "aborted_guard");
    Stage = "v38_dsi_done";
    SaveReport(FALSE);
}

static VOID ProbeV38Intf(ULONG_PTR mdss, ULONG_PTR sampledIntf)
{
    ULONG_PTR intf = At(mdss, 0x36000);
    ULONG value;
    Stage = "v38_intf";
    SaveReport(FALSE);
    if (sampledIntf == intf) {
        /* ProbeR34 already attempted this exact register; avoid a second read. */
        Line("v38_intf_config2_source=r34_intf060\r\n");
    } else {
        Line("v38_intf_config2_source=v38_intf_config2\r\n");
        (VOID)V38Word("v38_intf_config2", At(intf, 0x60),
            V38MmioReady(mdss, At(intf, 0x60)), TRUE, &value);
    }
    Stage = "v38_intf_done";
    SaveReport(FALSE);
}

static VOID Probe(VOID)
{
    ULONG_PTR base = 0, obj = 0, slots[4] = {0}, mdss = 0, dsi = 0, p;
    ULONG_PTR g2274d0 = 0, tables[2] = {0};
    ULONG imageSize = 0, i, j, value, ts1 = 0, ts2 = 0;
    ULONG data285220 = 0;
    UCHAR block[20];
    ULONG disp = 0;
    ULONG_PTR intf = 0;
    BOOLEAN tokenOk = FALSE, advanced = FALSE, mmioOk = TRUE;
    BOOLEAN mmioGate, globalOk = FALSE;
    static const ULONG offsets[] = {0x54,0x58,0x5C,0x60,0x2A4,0x2A8,0x2AC,0x2B0};
    static const ULONG tableOffsets[] = {0x3C0,0x3C8};
    static const PCSTR tableKeys[] = {"tbl3c0", "tbl3c8"};
    LARGE_INTEGER delay;
    Line("probe_version=3.8\r\nS_fields=unavailable\r\n");
    /* No validated mode-structure S pointer path exists in the base source. */
    if (!FindImage(&base, &imageSize)) return;
    Line("qcdxkm_base=0x%llx\r\nqcdxkm_size=0x%08x\r\n", (ULONGLONG)base, imageSize);
    if (!KernelRange(base, imageSize) || imageSize < 0x284840 + sizeof(slots)) {
        Fail("image_bounds");
        base = 0;
    }
    for (i = 0; i < RTL_NUMBER_OF(slots); ++i) {
        p = At(base, 0x284840 + i * sizeof(ULONG_PTR));
        if (Read(p, &slots[i], sizeof(slots[i]), FALSE)) {
            Line("obj[%u]=0x%llx\r\n", i, (ULONGLONG)slots[i]);
            if (!obj && !(slots[i] & 7) && KernelRange(slots[i], 0xB48) &&
                Read(slots[i], block, sizeof(ULONG_PTR), FALSE) &&
                Read(At(slots[i], 0x280), block, sizeof(block), FALSE) &&
                Read(At(slots[i], 0xB38), block, 16, FALSE)) {
                obj = slots[i];
                disp = i;
            }
        } else { Line("obj[%u]=unavailable\r\n", i); Fail("object_slots"); }
    }
    if (!obj) Fail("object_validation");
    Line("selected_obj=0x%llx\r\nx19=0x%llx\r\n", (ULONGLONG)obj,
        (ULONGLONG)At(obj, 0x284));
    Word("word_count", At(obj, 0x2CC), &value, "word_count");
    for (i = 0; i < 20; ++i) {
        if (Read(At(obj, 0x35C + i), block, 1, FALSE))
            Line("dsc[%u]=0x%08x\r\n", i, (ULONG)block[0]);
        else { Line("dsc[%u]=unavailable\r\n", i); Fail("dsc_record"); }
    }
    Word("dsc_enable", At(obj, 0x36C), &value, "dsc_enable");
    for (i = 0; i < 16; ++i) {
        if (Read(At(obj, 0xB38 + i), block, 1, FALSE))
            Line("token[%u]=0x%08x\r\n", i, (ULONG)block[0]);
        else { Line("token[%u]=unavailable\r\n", i); Fail("token_block"); }
    }
    Word("token_timestamp", At(obj, 0xB3C), &value, "token_timestamp");
    Word("token_state", At(obj, 0xB40), &value, "token_state");
    for (i = 0x280; i < 0x380; i += 4) {
        if (Read(At(obj, i), &value, sizeof(value), FALSE))
            Line("context[0x%03x]=0x%08x\r\n", i, value);
        else { Line("context[0x%03x]=unavailable\r\n", i); Fail("context"); }
    }
    Stage = "kernel_fields_done";
    SaveReport(FALSE);
    ProbeV38Panel(base, imageSize, disp, obj != 0);
    if (Read(At(base, 0x227558), &mdss, sizeof(mdss), FALSE)) {
        Line("mdss_ptr=0x%llx\r\n", (ULONGLONG)mdss);
        dsi = At(mdss, 0x94000);
        if ((mdss & 7) || !KernelRange(mdss, 1) || !KernelRange(dsi, 0x2B4)) {
            Fail("mdss_validation"); dsi = 0;
        }
    } else { Line("mdss_ptr=unavailable\r\n"); Fail("mdss_pointer"); }
    Line("dsi0_va=0x%llx\r\n", (ULONGLONG)dsi);
    Stage = "mdss_done";
    SaveReport(FALSE);
    if (Read(At(base, 0x2274D0), &g2274d0, sizeof(g2274d0), FALSE)) {
        Line("g2274d0=0x%llx\r\n", (ULONGLONG)g2274d0);
        globalOk = KernelRange(g2274d0, 0x3D0) &&
            Read(g2274d0, &p, sizeof(p), FALSE);
        if (!globalOk) Fail("g2274d0_validation");
    } else { Line("g2274d0=unavailable\r\n"); Fail("g2274d0_pointer"); }
    for (i = 0; i < RTL_NUMBER_OF(tables); ++i) {
        if (globalOk && Read(At(g2274d0, tableOffsets[i]), &tables[i], sizeof(tables[i]), FALSE))
            Line("%s_ptr=0x%llx\r\n", tableKeys[i], (ULONGLONG)tables[i]);
        else {
            tables[i] = 0;
            Line("%s_ptr=unavailable\r\n", tableKeys[i]); Fail("table_pointer");
        }
    }
    for (i = 0; i < RTL_NUMBER_OF(tables); ++i) {
        for (j = 0; j < 8; ++j) {
            if (KernelRange(tables[i], 8 * sizeof(ULONG)) &&
                Read(At(tables[i], j * sizeof(ULONG)), &value, sizeof(value), FALSE)) {
                Line("%s[%u]=0x%08x\r\n", tableKeys[i], j, value);
            } else { Line("%s[%u]=unavailable\r\n", tableKeys[i], j); Fail("table_dwords"); }
        }
    }
    Word("data_285220", At(base, 0x285220), &data285220, "data_285220");
    Stage = "tables_done";
    SaveReport(FALSE);
    if (Word("ts1", At(obj, 0xB3C), &ts1, "token_ts1")) {
        delay.QuadPart = -150LL * 10000LL;
        if (KeDelayExecutionThread(KernelMode, FALSE, &delay) == STATUS_SUCCESS)
            tokenOk = Word("ts2", At(obj, 0xB3C), &ts2, "token_ts2");
        else { Fail("token_delay"); Line("ts2=unavailable\r\n"); }
    } else Line("ts2=unavailable\r\n");
    advanced = tokenOk && ts2 != ts1;
    Line("token_advanced=%u\r\n", (ULONG)advanced);
    Stage = "pre_mmio";
    SaveReport(FALSE);
    /* Token progress is informational. Never map or power up hardware. */
    mmioGate = obj != 0 && dsi != 0 && KernelRange(mdss, 0x95000);
    if (mmioGate) {
        if (!Read(At(dsi, 0x58), &value, sizeof(value), TRUE)) {
            Line("mmio_probe58=unavailable\r\n");
            Fail("mmio_probe");
            mmioOk = FALSE;
        } else Line("mmio_probe58=0x%08x\r\n", value);
    }
    for (i = 0; i < RTL_NUMBER_OF(offsets); ++i) {
        if (mmioGate && mmioOk) {
            if (Read(At(dsi, offsets[i]), &value, sizeof(value), TRUE)) {
                Line("mmio[0x%03x]=0x%08x\r\n", offsets[i], value);
                continue;
            }
            Fail("mmio_read"); mmioOk = FALSE;
        }
        Line("mmio[0x%03x]=unavailable\r\n", offsets[i]);
    }
    Line("mmio=%s\r\n", !mmioGate ? "skipped" : mmioOk ? "ok" : "error");
    ProbeV38Dsi(mdss, mmioGate && mmioOk);
    if (!mmioGate || !mmioOk) return;
    ProbeGx3(base, mdss, &intf);
    ProbeR34(base, disp, obj != 0, mdss, intf);
    ProbeV38Intf(mdss, intf);
}

/* The report is flushed at several stages so that a hang in a later stage
 * (notably the MMIO reads) cannot erase the evidence already gathered.
 * The status line is appended only for the write and then rolled back,
 * so repeated flushes do not accumulate status lines in the buffer. */
static VOID SaveReport(BOOLEAN final)
{
    UNICODE_STRING path = RTL_CONSTANT_STRING(L"\\SystemRoot\\DscProbe_out.txt");
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK iosb;
    HANDLE file;
    NTSTATUS s;
    SIZE_T savedUsed;
    InitializeObjectAttributes(&attributes, &path,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    s = ZwCreateFile(&file, FILE_WRITE_DATA | SYNCHRONIZE, &attributes, &iosb,
        NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OVERWRITE_IF,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(s)) {
        Fail("report_open");
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DscProbe: status=error_report_open ntstatus=0x%08x\n", s);
        return;
    }
    savedUsed = Used;
    if (final) {
        if (Error) Line("status=error_%s\r\n", Error);
        else Line("status=ok\r\n");
    } else {
        Line("status=in_progress stage=%s\r\n", Stage);
    }
    s = ZwWriteFile(file, NULL, NULL, NULL, &iosb, Report, (ULONG)Used, NULL, NULL);
    if (!NT_SUCCESS(s) || iosb.Information != Used)
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DscProbe: status=error_report_write ntstatus=0x%08x\n", s);
    Used = savedUsed;
    Report[Used] = '\0';
    ZwClose(file);
}

VOID ProbeWorker(PVOID ioObject, PVOID context, PIO_WORKITEM item)
{
    UNREFERENCED_PARAMETER(ioObject);
    UNREFERENCED_PARAMETER(context);
    /* PASSIVE_LEVEL is structural: Probe runs only from the IoQueueWorkItem
     * callback. (KeGetCurrentIrql is not exported by the 28000 ARM64 kernel
     * library — LNK2019 — so no runtime IRQL check is linked in.) */
    NT_ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
    __try { Probe(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { Fail("probe_exception"); }
    SaveReport(TRUE);
    IoUninitializeWorkItem(item);
    ExFreePoolWithTag(item, TAG);
    KeSetEvent(&Finished, IO_NO_INCREMENT, FALSE);
    /* The I/O manager holds the driver reference until this callback returns. */
}

VOID ProbeUnload(PDRIVER_OBJECT driver)
{
    UNREFERENCED_PARAMETER(driver);
    KeWaitForSingleObject(&Finished, Executive, KernelMode, FALSE, NULL);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registryPath)
{
    PIO_WORKITEM item;
    UNREFERENCED_PARAMETER(registryPath);
    item = ExAllocatePool2(POOL_FLAG_NON_PAGED, IoSizeofWorkItem(), TAG);
    if (item == NULL) return STATUS_INSUFFICIENT_RESOURCES;
    KeInitializeEvent(&Finished, NotificationEvent, FALSE);
    IoInitializeWorkItem(driver, item);
    driver->DriverUnload = ProbeUnload;
    IoQueueWorkItemEx(item, ProbeWorker, DelayedWorkQueue, NULL);
    return STATUS_SUCCESS;
}
