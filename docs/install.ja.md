[English](install.md) | 日本語

# 導入ガイド — Nothing Phone (1) へ Windows 11 ARM64

> **下書き。** プロジェクトの作業ノートから組み立てたもの。全体の流れは
> 作者の実機で実証済みだが、細部のコマンドライン（ドライブ文字、イメージ
> のインデックスなど）は環境ごとに異なる。先に
> [../STATUS.ja.md](../STATUS.ja.md) を読んで、何が手に入るか把握してから
> 始めること。

## 0. 必要なもの

- ブートローダのアンロック済みの Nothing Phone (1)（A063）と PC。
- [Mu-Silicium フォーク](https://github.com/frost-1256/Mu-Silicium) の UEFI
  イメージ（CI で `Mu-spacewar.img` がビルドされる。DSDT は
  [Silicium-ACPI フォーク](https://github.com/frost-1256/Silicium-ACPI) の
  ものが組み込まれる）。
- Windows 11 ARM64 (22H2) のインストールイメージ。
- Qualcomm SM7325 向けドライバパック（いわゆる A52s パック）。ここでは
  配布しないので各自で入手すること。
- 自分用のコード署名テスト証明書（自己署名でよい）。電話側の Windows に
  信頼させる。
- [spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680) の
  タッチドライバ。

## 1. パーティションと Windows のインストール

作者の端末で使っている配置（userdata 領域から切り出したもの）：

| パーティション | サイズ | 内容 |
|---|---|---|
| ESP | 512 MiB、FAT32 | UEFI から起動する Windows のブートファイル |
| WIN | 64 GiB、NTFS | Windows 本体 |

Windows イメージは DISM で WIN へ展開し、`bcdboot` で ESP にブートファイルを
作る、という通常の WoA の流れ。（このインストール自体は本ドキュメントより
前に行われたもので、パーティション作成の手順は環境依存として扱うこと。）

## 2. UEFI の起動

UEFI は**フラッシュしない**。毎回 PC から起動する：

```console
fastboot boot Mu-spacewar.img
```

Android は自分のスロットに手付かずで残る。Windows が固まっても、再起動
すれば通常通り Android／fastboot に戻れる。

## 3. テスト署名

パッチしたドライバと自作ドライバにはテスト署名が要る：

- 自分用のテスト証明書を作り、署名用に電話側の `CurrentUser\My` と、
  マシンの信頼ストアへ入れる。
- BCD でテスト署名を有効化（`bcdedit /set testsigning on`）。
- カーネルデバッグは普段は**オフ**（`bcdedit /debug off`）。オンのままだと
  ホストが居ない KD トランスポート待ちでブートが止まる。デバッグ時だけ
  `tools/phone/kd_on.bat` を使う。

## 4. ドライバパックとパッチ

1. 対象ドライバを入れる**前に**、手元パックへパッチ仕様を適用する
   （[../patches/](../patches/) を参照）：
   - `qcwlan7325.sys` → 変種 `sarv2-allow`、加えて `fwVersion` の INF 変更
     （`00228` → `00816`）。この 2 つで Wi-Fi が起動するようになる。
   - `qcdxkm7325.sys` → 表示の調査に加わるなら変種 `dsc2`。タッチ・Wi-Fi・
     Bluetooth だけの実用構成なら GPU は不要。
2. パッチしたドライバ（と INF を触ったドライバ）は、全部自分のテスト
   証明書で再署名する。
3. パックは管理者プロンプトから `pnputil /add-driver *.inf /subdirs /install`
   で入れ、タッチドライバはそのリポジトリの手順で入れる。

インストール済みの保護ドライバを差し替えるのは、オフライン（ガジェット
モード）のボリューム経由で DriverStore へ再署名済みファイルを書く場合
だけ。稼働中セッションから DriverStore の実ファイルを直接上書きしては
ならない。

## 5. GPU の基準状態：ブート時はオフに保つ

まず、グラフィックスデバイスの `ConfigFlags` を 1（ブート時無効）に
設定する：

```text
HKLM\SYSTEM\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0
  ConfigFlags (REG_DWORD) = 1
```

有効化は、サインイン画面の静穏な窓で手動でだけ（`tools/phone/gpu_now.bat`）、
表示実験のために行い、終わったらすぐ無効化する。**有効化のたびに、次の
再起動前に `ConfigFlags` が 1 に戻っていることを確認すること** — 有効化が
中断されると 0 のまま残り、次のブートで開始時に GPU が起きて凍結しうる。
背後にある欠陥については [../STATUS.ja.md](../STATUS.ja.md) を参照。

## 6. 初回ブートのチェックリスト

- ログオン画面まで起動し、サインインできる（PIN の設定を推奨。ブートが
  サインイン画面で止まるようになり、それが GPU テストに使う静穏な窓にも
  なる）。
- タッチが効く。`pnputil /scan-devices` の後でも効く（ドライバが自分の
  ピン mux を復元する）。
- Wi-Fi がネットワークを表示して通常速度で接続でき、Bluetooth が
  ペアリングと音声出力まで行ける。
- デバイスマネージャーでグラフィックスデバイスが無効になっている。
