[English](install.md) | 日本語


# 導入と安全な保守

対象は Nothing Phone (1) A063 / SM7325、Windows 11 ARM64 **22H2**、2026-11-05 時点のフォーク構成。変更前に[動作状況](../STATUS.ja.md)と[既知の問題](known-issues.ja.md)を読む。このガイドは実証済みの導入モデルを記録するもので、今回新たにクリーンインストールを検証したものでも、全端末共通の再パーティション手順でもない。

## 1. 前提条件と復旧の準備

- アンロック済み A063、fastboot を使える PC、安定した USB 接続、fastboot／リカバリーへの復帰手段。
- Android データ、元のパーティションテーブル、選んだ配置を元に戻すためのパーティションイメージのバックアップ。userdata の再分割は破壊的操作で、アンロックでもデータが消える場合がある。
- [Mu-Silicium フォーク](https://github.com/frost-1256/Mu-Silicium)の `Mu-spacewar.img` と、[Silicium-ACPI フォーク](https://github.com/frost-1256/Silicium-ACPI)のボードテーブル。Bluetooth の実証済み Serial Bus 修正は DSDT v21 に含まれるため、その修正とタッチ対応を保持したビルドを使う。
- Windows 11 ARM64 22H2 イメージ、DISM／BCDBoot、別途入手した Qualcomm SM7325／A52s ドライバパック。
- パッチ適用用 Python 3、署名ツール、変更したカーネルバイナリとカタログに使う自分のテスト署名用 ID。
- [FT3680 タッチドライバ](https://github.com/frost-1256/spacewar-ft3680)。ビルド・導入は同リポジトリの手順に従う。

原本パックと最後に起動できたファームウェア／パッケージ一式はリポジトリ外に残す。実験前に、電話の Windows が動いていない状態で、ガジェットモードから Windows ボリュームと ESP を公開できることを確認する。復旧は、凍結した Windows 内の GPU、Wi-Fi、タッチに依存させない。

## 2. ストレージ配置とイメージ展開

実証済みの配置では userdata から領域を切り出している。

| パーティション | サイズ／形式 | 用途 |
|---|---|---|
| ESP | 512 MiB / FAT32 | Windows の UEFI ブートファイルと BCD |
| WIN | 64 GiB / NTFS | Windows システムボリューム |

元の縮小・作成コマンド、アラインメントの判断、変更前後の GPT 全体は記録に残っていない。推測した DiskPart スクリプトや別端末のセクタ位置を実行せず、実際の配置、バックアップ、復旧手順を先に確定する。上のサイズは動作例であり、最低要件や空き容量の保証ではない。

対象の電話パーティションだけを保守用にマウントし、一時的なドライブ文字を実物と照合する。以下では WIN を `W:`、ESP を `S:`、導入メディアを `I:` とするが、実際に検出した文字ではなく置換用の例。適用前にイメージを調べて ARM64 エディションのインデックスを選ぶ。

```bat
dism /Get-WimInfo /WimFile:I:\sources\install.wim
```

`<ARM64_INDEX>` を選んだ番号に置換する。ESD や分割イメージならファイル名を変えず、DISM の入力オプションを合わせる。

```bat
dism /Apply-Image /ImageFile:I:\sources\install.wim /Index:<ARM64_INDEX> /ApplyDir:W:\
bcdboot W:\Windows /s S: /f UEFI
```

これは標準の保守コマンドのひな型で、元の導入ログの復元ではない。作成された BCD が電話の Windows を指すことを確認し、以降はそのストアを明示して設定する。電話用の操作で保守 PC の BCD を変更しない。

## 3. 起動モデル

Windows を起動するたびに fastboot から実行する。

```console
fastboot boot Mu-spacewar.img
```

UEFI はメモリへ読み込まれ、電話のブートスロットには書き込まれない。実証済みの配置では Android は既存スロットに残る。再起動で Android／fastboot に戻れても、パーティション変更や GPT 破損は元に戻らない。端末単体の A/B デュアルブートは将来構想で、ここにフラッシュ手順はない。Linux 導入時の DTBO 消去などは、この Windows 導入手順には含まれない。

## 4. テスト署名とデバッガの既定値

SYS の変更で埋め込み署名が無効になり、SYS または INF の変更でカタログのハッシュも無効になる。SYS への署名だけでは古いカタログは修復されない。バイト列・INF の変更をすべて終え、適切な WDK のパッケージツールで対象カタログを再生成し、バイナリとカタログに署名して、完成したパッケージを検証してから登録する。不一致の元 CAT は再利用しない。

テスト証明書は自分で作り、秘密鍵は署名するシステムに保持する。署名スクリプトは `CurrentUser\My` を使うが、秘密鍵を電話へコピーする必要はない。電話には、そのテスト ID に必要な公開証明書だけを Local Machine の「信頼されたルート証明機関」と「信頼された発行元」へ登録する。このリポジトリでは証明書も鍵も配布しない。

電話で動作中の Windows の管理者プロンプトから設定する場合：

```bat
bcdedit /set testsigning on
bcdedit /debug off
```

オフライン設定では、電話のストアを指定し、先にローダー一覧を確認する。

```bat
bcdedit /store S:\EFI\Microsoft\Boot\BCD /enum all
bcdedit /store S:\EFI\Microsoft\Boot\BCD /set {PHONE_LOADER_GUID} testsigning on
bcdedit /store S:\EFI\Microsoft\Boot\BCD /set {PHONE_LOADER_GUID} debug off
```

識別子はそのストアにある Windows ローダーの実値に置換する。カーネルデバッグは**既定でオフ**にする。設定済みトランスポートの初期化が、接続していないデバッガを待つ場合がある。`bootdebug` は別の設定で、それだけを変更してもこの待ちは解消しなかった。`kd_on.bat` は意図したデバッグ時だけ使い、終了後は `kd_off.bat` に戻す。テスト署名とファームウェアのブートセキュリティ設定は整合が必要だが、全環境共通の Secure Boot 設定手順は資料にない。

## 5. ドライバの準備順序

1. 原本パックを保存し、対象2ファイルのハッシュを[パッチ仕様](../patches/README.ja.md)と照合する。別ビルドは再監査が必要で、適用スクリプトの拒否を回避しない。
2. 原本 WLAN SYS に `sarv2-allow` を適用し、UTF-16LE の `qcwlan7325.inf` で `WlanFWVer` を `00228` から `00816` に変える。バージョンゲートと SAR 初期化は別の障害なので両方必要。
3. 表示調査をする場合は、原本グラフィックス SYS から直接 `dsc2` を作る。`capw` と `dsc1` は累積適用されるため、適用済みファイルへ順番にスクリプトを実行しない。タッチ・Wi-Fi・Bluetooth の基準構成の維持に GPU は不要。
4. INF 編集を終え、対象カタログを再生成し、電話で信頼させたテスト ID で変更パッケージを再署名する。入力ハッシュ、変種、完成後ハッシュ、対応ファームウェア／ACPI ビルドを記録する。
5. 意図したパック構成に合わせてボード用プロバイダと拡張 INF を登録・導入する。拡張 INF をコピーしただけで設定が適用されたと考えない。別配布のタッチパッケージはそのリポジトリの手順で入れる。
6. GPU と ADSP を初めて開始させる**前に**、両方を無効にした基準構成を作る。パック内の全デバイスを一律に有効にしない。ADSP のブート時開始は既知のクラッシュを起こす。

リポジトリのルートから、別途入手したファイルへ適用する例：

```console
python3 patches/apply_patches.py qcwlan7325.sys sarv2-allow
python3 patches/apply_patches.py qcdxkm7325.sys dsc2
```

初回のオフライン準備では、意図して用意したドライバディレクトリを DISM で電話のイメージへ登録できる。

```bat
dism /Image:W:\ /Add-Driver /Driver:<PREPARED_PACK_DIRECTORY> /Recurse
```

動作中の電話へ通常のパッケージ導入をする場合、既存の流れでは準備済みディレクトリ内の管理者プロンプトで次を使う。

```bat
pnputil /add-driver *.inf /subdirs /install
```

`/install` は一致したデバイスを直ちに開始する場合がある。無効化の基準状態を先に作り、初回起動の GPU／ADSP へ無差別に適用せず、パッケージを選ぶ。導入後はバインドと拡張 INF の適用を確認する。ドナーパック全プロバイダの普遍的な開始順序は記録にないため創作しない。ADSP に関して具体的に必要なのは、ADSP の QMI 使用**より前に** SSDD 登録が完了すること。

## 6. 安全な基準状態の設定と確認

このボード構成のグラフィックスインスタンス：

```text
ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0
```

動作中の Windows では無効化し、現在のコントロールセットを確認する。

```bat
pnputil /disable-device "ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0"
reg query "HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0" /v ConfigFlags
```

期待する基準値は `ConfigFlags REG_DWORD 0x1` で、デバイスマネージャーでも無効になっていることを確認する。実証済みのオフラインキーは次のとおり。

```text
HKLM\SYSTEM\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0
  ConfigFlags (REG_DWORD) = 1
```

オフライン編集では電話の SYSTEM ハイブを読み込み、`Select\Current`／`Default` を調べ、次回通常起動で使うコントロールセットを対象にする。別環境でも ControlSet001 とは限らない。[オフライン復旧例](known-issues.ja.md)にバックアップ、アンロード、読み戻しまで記載する。ADSP もブート無効を維持し、`ACPI\QCOM0A1B` または対応する拡張 ID の実インスタンスを調べる。末尾のインスタンス番号は推測しない。

GPU テストは手動で、できればサインイン画面の静かな時間に短く行い、直後に無効化する。`gpu_now.bat` は有効化して5分間記録するだけで、後片付けしない。`pnputil /enable-device` は ConfigFlags=0 を永続的に残しうる。**有効化のたびに再起動前に再照会し、クラッシュで無効化できなかった場合は次の Windows 起動前にオフライン修復する。**

## 7. 初回起動の確認項目

- カーネルデバッグがオフの状態でログオンまで到達し、サインインできる。PIN を設定すると後の実験用にサインイン画面で止めやすい。
- GPU が無効で ConfigFlags=1、ADSP もブート時無効になっている。
- タップ、リリース、長押しができ、再スキャン（`pnputil /scan-devices`）後もタッチが効く。
- Wi-Fi がネットワークを検出して通常速度で接続でき、Bluetooth が検出・ペアリング・ヘッドセット音声出力まで動く。
- ストレージ、ボタン、充電が動く。バッテリー残量がないのは現状の想定どおり。
- 電話の Windows を起動せず、ガジェットモードから BCD と SYSTEM ハイブを確認できる。

表示実験前に OS バージョン、ファームウェア／DSDT の識別情報、導入パッケージ版、適用パッチを記録する。これらの確認が通っても、DSC の正常表示に成功したことにはならない。

## 8. 差し替えとトラブルシューティング

**DriverStore の差し替え規則：** 電話の Windows が動いている間は、デバイスを無効にした後でも DriverStore の保護ファイルを直接上書きしない。過去のライブ差し替えではファイルが失われ、オフライン復旧が必要になった。このプロジェクトの直接差し替えでは、電話の Windows を停止し、ガジェットモードで公開して対象パッケージをバックアップし、正しく再署名した整合するファイルをオフラインでコピーしてハッシュ・署名を確認し、安全にアンマウントしてから起動する。新パッケージ導入には通常のパッケージ保守を使い、別リビジョンの SYS／INF／CAT を混ぜたり、似た名前の DriverStore フォルダを推測で全部編集したりしない。

| 症状 | 確認・対処 |
|---|---|
| デバッガ不在でブートが待つ | 電話ローダーの `debug` を確認し通常起動ではオフにする |
| GPU 有効化後、次回から凍結 | Windows を再試行する前にオフラインで ConfigFlags を確認し、後片付け未完なら1へ戻す |
| GPU 無効でも凍結 | 重いブート初期処理でも 0x101 は起こる。証拠とフラグを確認し、自動有効化の繰り返しを避ける |
| 新パッケージが読み込めない | 原本ハッシュ・変種、SYS 署名、再生成 CAT、INF 文字コード、電話の信頼ストア、BCD を確認 |
| WLAN が開始できない | fwVersion `00816` と SAR v2 パッチの両方、実際の拡張 INF／ファームウェアのバインドを確認 |
| Bluetooth が Code 10 | 組み込み DSDT の Serial Bus RevisionID 修正を確認。iasl マクロ名だけでなく実バイトを見る |
| 再スキャン後タッチ不能 | GPIO52–55 自動復元の統合版を確認。実行中の生の電源投票を追加しない |
| ADSP がブート時 0x0A | ブート無効へ戻し、SSDD 完了後の実行時テストにする |
| 表示が構造のあるノイズ | 未解決の dsc2 の症状。監査済み定数を無作為に変更しない |

ブートが凍結したらガジェットモード／fastboot へ復帰し、残ったレポートを保存して GPU と ADSP の開始状態を確認する。凍結型の watchdog リセットでは新しいダンプが残らない場合がある。古い 0x0A ダンプを最新障害と誤認しないよう時刻を照合する。機構と復旧の限界は[既知の問題](known-issues.ja.md)を参照。
