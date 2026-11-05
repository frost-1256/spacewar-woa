[English](README.md) | 日本語


# Nothing Phone (1) で Windows 11 — spacewar

Nothing Phone (1)（型番 **A063**、コードネーム **spacewar**、Snapdragon 778G / **SM7325**）で、作者の Project Silicium フォークを使って Windows 11 ARM64 **22H2** を動かすための公開ドキュメント。

**2026-11-05 時点の状況：** UEFI を `fastboot boot` で読み込む構成で Windows のログオンまで起動し、タッチ、通常速度の Wi-Fi、Bluetooth、USB ガジェット、充電、ボタン、UFS ストレージが動く。`dsc2` グラフィックスドライバは開始してパネルを駆動するが、Display Stream Compression（DSC）の出力は構造のあるノイズとして復号され、ドライバ経由の正常表示にはまだ到達していない。GPU はブート時無効とし、別途発生するプロセッサ間ランデブーのフリーズ（BugCheck 0x101）も調査している。音声、バッテリー情報など、未動作の機能も多い。

## 読む順序

| 文書 | 内容 |
|---|---|
| [STATUS.ja.md](STATUS.ja.md) | サブシステム別の動作状況と基準構成の限界 |
| [docs/install.ja.md](docs/install.ja.md) | 前提条件、イメージ展開、署名、パッチ順序、安全な初回起動 |
| [docs/known-issues.ja.md](docs/known-issues.ja.md) | フリーズの機構、ConfigFlags の罠、ADSP の開始順序と復旧 |
| [docs/display-investigation.ja.md](docs/display-investigation.ja.md) | 証拠、レジスタ比較、除外済みの線と今後の実験 |
| [docs/subsystems.ja.md](docs/subsystems.ja.md) | タッチ、Wi-Fi、Bluetooth、ADSP、バッテリー、USB の詳細 |
| [patches/README.ja.md](patches/README.ja.md) | 検証済みパッチ表、累積変種、パッケージの変更 |
| [tools/README.ja.md](tools/README.ja.md) | 電話側の補助スクリプトと運用上の限界 |
| [tools/dscprobe/README.ja.md](tools/dscprobe/README.ja.md) | 読み取り専用表示プローブのソース、ビルド、採取手順 |

各文書には全文に対応する英語版があり、冒頭から切り替えられる。調査文書では **PROVEN（確定）**、**EXCLUDED（除外）**、**INFERENCE（推論）**、**OPEN（未確定）**、**PLANNED（計画）**を区別する。ドライバの開始成功だけでは、正しい画素の出力やプラットフォームの安定性は保証されない。

## このリポジトリの範囲

含むものは、文書、機械可読のバイナリパッチ仕様、ハッシュ照合付き適用スクリプト、DscProbe のソースとビルド補助、小規模な電話側スクリプト。

含まないものは、Qualcomm ドライバの原本・パッチ済みバイナリ、ファームウェア一式、Windows イメージ、プローブの生成結果、署名証明書、秘密鍵。ドライバパックと OS は別途入手し、成果物は各自でビルド・署名する。適用可否を決めるのはファイル名や「SM7325 パック」という呼び名ではなく、[patches.json](patches/patches.json) の原本ハッシュ。ビルド成果物、ログ、バックアップはリポジトリ外で管理し、既存の ignore 保護も維持する。

## 関連プロジェクトと役割

- [frost-1256/Mu-Silicium](https://github.com/frost-1256/Mu-Silicium)：この端末で使う UEFI フォーク。上流の spacewar は Inactive 扱いで、ここで説明する Windows 起動はこのフォークの構成によるもの。
- [frost-1256/Silicium-ACPI](https://github.com/frost-1256/Silicium-ACPI)：ACPI のボード記述。DSDT v21 で出荷した Serial Bus 記述子のリビジョン修正も含む。
- [frost-1256/spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680)：GPIO mux 自動復元を備えた FocalTech FT3680 タッチドライバ。

ACPI 修正、別リポジトリのタッチドライバ、ドライバパックのバイナリパッチはそれぞれ別の層にある。パッチ JSON の適用だけではボード全体の構成を再現できない。

## 実験端末としての注意

再パーティション前にデータとパーティション配置をバックアップする。ブートローダのアンロックと復旧経路の確保が必要な実験的移植であり、フリーズすると fastboot／EDL での復旧が必要になる場合や、ダンプが残らない場合がある。UEFI を RAM 起動しても、その前に行ったパーティション変更が元に戻るわけではない。導入ガイドは現在の `fastboot boot` 構成を対象とし、UEFI のフラッシュや Android のブート情報の消去は指示しない。

**GPU を有効にするたびに**、終了後は無効化し、**再起動前に ConfigFlags=1 を確認する**。クラッシュで後片付けが中断すると ConfigFlags=0 が永続的に残り、次回ブートでも GPU が開始される。[復旧手順](docs/known-issues.ja.md)を参照。

## 将来の構想 — 未着手

A/B スロットによる端末単体の Android／Windows デュアルブート、22H2 安定後の Windows 大型バージョン対応、Qualcomm ドライバのクリーンルームによるオープンソース再実装は将来の構想。現時点の対応済み導入経路でも、提供を約束した成果物でもない。
