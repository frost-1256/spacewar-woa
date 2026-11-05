[English](subsystems.md) | 日本語


# サブシステムの技術メモ

**2026-11-05 時点**の spacewar A063 / SM7325、Windows 11 ARM64 22H2、関連フォークを対象とする。一覧は[動作状況](../STATUS.ja.md)、復旧は[既知の問題](known-issues.ja.md)を参照。上流プロバイダが動いても下流機能がすべて動くとは限らない。

## 1. タッチ：FT3680 とピン mux 自動復元

動作するドライバは [spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680)。PnP 再スキャン後のタッチ不良は当初レール断とされたが、実測では GPIO52–55 が mux 0→1 になり、GPIO53–55 の output-enable が解除されていた。書き換え元は不明。この再スキャン障害についてのレール崩壊説は撤回している。

統合済み v3 は**各通信の直前**、既存の **BusMutex** 内で該当 CTL を確認し、観測済みの mux／OE のずれだけを既知の正常値へ戻す。デバイスの無効化・再ロード、コントローラ reset、電源投票はせず、GPIO81/105、Resource State Coordinator（RSC）、System Power Management Interface（SPMI）も操作しない。電源ポリシー所有権は FALSE のまま。ピン割り当て修復のためにプラットフォーム電源管理と競合するのを避けた狭い設計になっている。

**PROVEN：** 再スキャン5回で復元10回すべて成功し、その後5分間は追加変更なし、タッチ復帰も目視確認した。修正はタッチの本線リポジトリへ統合済み。後の通常速度 Wi-Fi とタッチの共存も確認されている。一方、長期常用、スリープ復帰、他トリガー、書き換え元の同定は、この試験では確定しない。

### 入力・報告経路の基準

現基準の HID descriptor は Tip Pressure を含めず、小さい ID／Tip／InRange／X／Y 構成。以前の pressure 付き構成はシェルの反復障害と対応し、除去で安定した。報告は parser 側 ghost filter と単純な parse→report を維持する。重複抑制は長押し／右クリックを壊し、dedupe を切ったまま release debounce を組み合わせると触れたままになった。報告経路を変えるなら、移動だけでなくタップ、リリース、**長押し**を必ず検証する。

bring-up では GPIO81 を入力／pull-up にし、100 ms ポーリングと drain 読みを使った。過去の記録では割り込み Low を確認できていない。DSDT v21 で SPI 接続自体は正常に開くが、記録上の転送基準はまだ `UseBb=1`。リソースを開けたことをハードウェア SPI 転送への移行成功とは扱わない。

以前のブート中の生 RSC 再投票は現役の電源モデルと競合し、SOC_CRITICAL_DEVICE_REMOVED を起こした。再スキャンの mux 障害を直すために復活させない。過去の電源モデル上の疑問と、現在動いている mux 復元は別件。

## 2. Wi-Fi：独立した開始ゲート2つ

SM7325 ドナーパックの期待ファームウェア文字列は `00228`、Nothing は **00816** を返す。UTF-16LE の `qcwlan7325.inf` で `WlanFWVer` を `00816` にすると、`[fwVersion.reg]` が `fwVersion` として登録する。これはバージョン不一致を通すだけで、SAR 初期化は直らない。

Nothing 純正 `bdwlan.elf` は Specific Absorption Rate（SAR）**v2** を広告する。ドライバの `SarInit` v2 分岐が `0xC0000001` を返して WLAN 開始全体を中断していた。`sarv2-allow` はこの分岐を成功返却へ変える。1命令のバイト置換は既存の[パッチ表](../patches/README.ja.md)を正本とし、レジストリだけではこの分岐の失敗を回避できなかった。

**PROVEN：** バージョン変更と SAR パッチで通常速度の Wi-Fi とタッチ共存が成立した。端末固有の bring-up 回避策で、SAR／電力ポリシー全体や別ファームウェアとの組み合わせの認証ではない。適切な純正ボードファームウェアを保ち、ドナーのメタデータを理由に無作為に無線校正を差し替えない。

`GlobalDebugLogOutput=1` の大量ログはカーネルデバッグ転送を埋め、極端に遅くなることが観測された。意図したデバッグ以外は `0` とし、ログ由来の遅さを開始ゲートと混同しない。最近の基準ブートでは必要な TrEE チェーンが自動開始し、旧 `tree_start` 手動手順は不要。

## 3. Bluetooth：ACPI Serial Bus リビジョン

DSDT **v21** は FTSX3680 の SPI と BTH0 の UART の Serial Bus 記述子だけを **RevisionID 2→1** に変更し、I2C 10件は変えない。2026-10-31、Bluetooth UART Transport Driver が Code 10 を抜け、開始、検出、ペアリング、接続、ヘッドセット音声を確認した。

正本は [Silicium-ACPI フォーク](https://github.com/frost-1256/Silicium-ACPI)。逆コンパイラが RevisionID=1 でも V2 マクロ名を表示することがあり、テキストをそのまま再コンパイルすると修正が戻りうる。生成した記述子のバイトを確認し、UART／SPI リソース変更と WLAN のバイナリパッチは分けて考える。SPI 接続成功も、前述のタッチ転送経路の主張を変えない。

Bluetooth ヘッドセット音声は動くが、内蔵スピーカー／マイクの経路は未対応。この2つをまとめて電話全体の音声が完成したとはしない。

## 4. ADSP：初期化の所有者とファームウェア経路

Audio DSP（ADSP）、Wireless Processor Subsystem（WPSS）などは `qcsubsys7325.sys` を共有するが、生成経路と設定が異なる。**Qualcomm Subsystem Dependency Device（SSDD）、QCOM0A20** が初回登録でグローバル QMI ミューテックス **image+0x76568** を初期化する。**Qualcomm Service Manager Device（QSM）、QCOM0A1E** は別の dispatch-table entry で、この初期化はしない。

クラッシュ位置 RVA **0x4B214** は埋め込みオブジェクトのアドレスで `KeWaitForSingleObject` を呼ぶ。対応する initializer は `KeInitializeMutex` を使い、有効なアドレスでも内部 wait-list が未初期化なら壊れる。反復したブート失敗とサインイン後の実行時成功で、判別条件は**開始順序**と分かった。共有 SYS のロード、QSM 有効化、SSDD の ConfigFlags=0 だけでは登録完了を示せない。

| パックの入力・経路 | ADSP | WPSS |
|---|---|---|
| 基本 hardware ID | ACPI\QCOM0A1B | ACPI\QCOM0AE2 |
| ファームウェア設定 | Audio 拡張 `ADSP\ImagePath=%13%\qcadsp7325.mbn` | main-image 動的取得、WLAN が `%13%\WPSS.MBN` を公開 |
| Binary resources | RADS.bin | RWPS.bin |
| 保護ドメイン | 2：`msm/adsp/audio_pd`、`msm/adsp/sensor_pd` | 調べた基準構成で0 |
| 必要なインターフェース | PIL_TZ, FASTRPC, GLINK | TFTP, PIL_TZ, GLINK |
| Volume-startup wait | WaitForVolStartupAppComplete=1 | WaitForVolStartupAppComplete=1 |

`%13%` は導入された**その拡張／パッケージ自身の DriverStore ディレクトリ**で、サブシステム SYS の隣の任意フォルダではない。Audio／PIL 拡張がボード ID に実際に一致する必要がある。hardware key の Resources／PDInfo、software key の ImagePath／interface list、service Parameters は別のスコープで、違う場所へコピーしてもパックの設定にはならない。

Windows パックは MBN を使い、`qcadsp7325.mbn` は存在する。PIL には open 失敗を検査する経路があり、loader status は `0xE0148204`。`.mdt`／`.bNN` 不足はこの QMI クラッシュの確立した説明ではない。ファイルの存在だけでは実導入、メモリ予約、セキュリティ受理、通信プロバイダまでは証明しないが、後の実行時開始成功は実際に通った開始経路を示す。

**2026-11-04 の PROVEN：** SSDD 開始済み → サインイン後に手動 ADSP 有効化 → Status=OK、問題コードなし、PIL のファームウェア開始成功。ブート時 ADSP → 同じ位置の0x0A。ADSP はブート無効を維持し、後で SSDD 準備後にだけ試す。全プロバイダの完全な順序や汎用のブート順修正は未実装。ADSP の保護ドメイン／service-registry 活動は WPSS より追加の QMI 経路を通る理由になるが、元クラッシュの全上位スタックは未取得。

## 5. バッテリー：チェーン開始とデータ配送は別

PMIC、PMIC GLink（`QCOM0A8E`）、バッテリーミニポート（`QCOM0A2A`）はすべて正常開始するが、**Win32_Battery は0件**。充電は Windows の容量・残量表示と独立して動く。電池転送は lpass 上の ADSP 側 `PMIC_RTR_ADSP_APPS` チャネルに関連するため、実行時 ADSP 開始は有用な実験だったが、データ到着の保証ではない。

チェーンは **ADSP 不在のブート時**に形成され、実行時停止・再開始を受け付けなかったため、後から ADSP が成功してもチェーンを作り直せなかった。ブート初期から正しい順序で ADSP が存在すれば電池データが来るかは**未検証**。電池ハードウェア不足の証明でも、残量計の動作確認でも、早期 ADSP で直るという確証でもない。

## 6. USB ガジェット、ホスト、Power Delivery

**動作するもの：** ガジェットモードで Windows／ESP を PC に公開でき、保護ドライバ、BCD、オフライン SYSTEM ハイブの保守・復旧に使う。電話の Windows を先に停止する。ストレージ公開は、稼働中にマウントしたファイルシステムの安全な共有とは別。

**動かないもの：** USB ホストと USB Power Delivery ネゴシエーション。URS0 が列挙されず、ドライバ導入に失敗した USB Type-C Connector System Software Interface（UCSI）デバイスへの ACPI `_DEP` が最有力説明。ただしこの依存関係説は**未確定**で、UCSI 導入だけでホスト／PD が直るとは示されていない。充電とガジェットストレージの成功も、この経路を検証しない。

## 7. その他の機能と範囲

UFS、ボタン、充電は実証済み。携帯モデム、Global Navigation Satellite System（GNSS）、NFC、カメラ、センサー、指紋、内蔵音声は未動作で、資料に各機能の確定した根本原因はない。ADSP 開始は基盤の前進で、全 consumer 実装の証拠ではない。将来のドライバ再実装や新 Windows 対応は構想で、欠けた機能の現時点の回避策ではない。
