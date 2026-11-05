[English](README.md) | 日本語

# Nothing Phone (1) で Windows 11 — spacewar

**Nothing Phone (1)**（A063、Snapdragon 778G / SM7325、コードネーム
**spacewar**）へ **Windows 11 ARM64 (22H2)** を移植する作業の記録、導入
手順、バイナリパッチ仕様、小物ツールをまとめたリポジトリ。土台は
Mu-Silicium のフォーク。

> **現状を一言で：** Windows はログオン画面まで起動し、**タッチ・Wi-Fi・
> Bluetooth が動作する**。残る大きな課題は表示出力で、GPU ドライバは起動
> してパネルを駆動するものの、Display Stream Compression（DSC）のスト
> リームが化けてしまうため、調査の間は GPU をブート時に無効化している。
> 詳細な一覧は [STATUS.ja.md](STATUS.ja.md) を参照。

## このリポジトリの中身

| パス | 内容 |
|---|---|
| [docs/install.ja.md](docs/install.ja.md) | 導入手順の全体 |
| [patches/](patches/) | バイナリパッチの**仕様**（オフセットと変更前後のバイト列）と適用スクリプト |
| [tools/](tools/) | 計測ドライバ DscProbe（ソース）と電話側の補助スクリプト |
| [STATUS.ja.md](STATUS.ja.md) | 動くもの・動かないもの・既知の欠陥 |

## このリポジトリに無いもの

- **ドライバのバイナリは置いていない**（元のものも、パッチ済みのものも）。
  Qualcomm の SM7325 向けドライバパックは再配布しないので、各自で入手
  して、手元のファイルにパッチ仕様を適用すること。
- **証明書や鍵も置いていない。** パッチしたドライバは各自のテスト証明書
  で再署名する。手順は導入ガイドに書いてある。

## 関連プロジェクト（同じ作者）

- [Mu-Silicium フォーク](https://github.com/frost-1256/Mu-Silicium) — この
  機種向けの UEFI ファームウェア（上流では spacewar は Inactive 扱いで、
  このフォークで Windows 起動まで持ってきている）
- [Silicium-ACPI フォーク](https://github.com/frost-1256/Silicium-ACPI) —
  DSDT などの ACPI テーブル（タッチ・Wi-Fi・Bluetooth の修正はここ）
- [spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680) —
  FocalTech FT3680 タッチドライバ

## 警告

実験的な作業であり、最悪の場合は fastboot／EDL での復旧が必要なところ
まで固まることがある。プラットフォームには断続的なフリーズ（BugCheck
0x101）という既知の欠陥があり、GPU の作業はまさにそれを追っている最中。
試す場合は自己責任で、自分の端末に対してのみ。
