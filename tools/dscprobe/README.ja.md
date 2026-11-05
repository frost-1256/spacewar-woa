[English](README.md) | 日本語

# DscProbe

Windows 11 ARM64 向けの、一度だけ実行する**読み取り専用**カーネル プローブ。表示が生きている状態で、spacewar 上の `qcdxkm7325.sys` の 表示／DSC 状態を調べるために使う。ハードウェア、対象ドライバのメモリ、 レジストリ、ネットワークへの書き込みは一切行わず、書くファイルは自分の レポート `C:\Windows\DscProbe_out.txt`（プレーンな `key=value` 形式） だけ。

現在のバージョンは **v3.8**。以前の版が読んでいたレジスタ集合（DSC エンコーダブロック、DSC／CTL／INTF の状態、ワードカウント、DSC レコード）に加えて、次を読む：

- panel-info の DSC フィールド群：主フラグ、モード index、固定 bit-clock override、bytes-in-slice、スライス数、DSC 設定の生ダンプ。
- DSI エラー／状態：FIFO、ACK、PHY、timeout、mask、割り込み制御、clock status。正常メインラインと比較するための集合。

`qcdxkm7325.sys` 内の固定 RVA を使うので、対象は [`../../patches`](../../patches) で同定される正確なドライバビルドに 限られる。すべての参照は境界チェックとガード付きで、結果は段階的に 書き出すため、後段のガードで失敗しても先に読んだ分は失われない。

## ビルド

ARM64 ターゲットの EWDK `LaunchBuildEnv` シェルから：

```bat
build.bat
sign.bat   :: sign.bat 内の証明書は自分のテスト証明書に置き換えること
```

## 使い方

`DscProbe.sys` を電話へコピーし（例：`C:\DscProbe\DscProbe.sys`）、 グラフィックスデバイスを有効化して表示が生きている状態で、管理者 プロンプトから：

```bat
sc.exe create DscProbe type= kernel start= demand binPath= C:\DscProbe\DscProbe.sys
sc.exe start DscProbe
type C:\Windows\DscProbe_out.txt
sc.exe stop DscProbe
sc.exe delete DscProbe
```


## 安全性の限界と採取の読み方

表示が生きていることは**実行する側が満たす前提条件**。境界・マップ済みページの検査や例外ガードは給電・クロックのロックではなく、停止ブロックへの読みで起きるバスハングを復旧できない。プローブは物理 MMIO の新規 map、電源投入、エラー acknowledge／clear、パターン注入を行わない。自動のブート初期プローブとして実行しない。token の進行は参考値で、正常画素の証明ではない。検証済みの mode-structure pointer がないため、現ソースは `S_fields=unavailable` と報告する。

サービス開始で一回限りの worker がキューに入り、`sc start` 成功は採取完了を意味しない。出力末尾の `status=` を確認する。段階保存は `status=in_progress stage=...`、ガード失敗では unavailable と最終 error が出る。途中や古い結果は新規採取成功として扱わない。次回で上書きする前に退避し、変種、DSC オン／オフ、ファームウェア／ACPI 識別、採取時点を記録する。

2026-11-05 の v3.8 基準採取では FIFO_STATUS=0x99991290、正常メインラインは0x11111210だった。そのスナップショットに HS レーン underflow 群がある証拠で、因果関係の証明ではない。他エラーはドライバが既に選択クリアしている場合があり、0でも全履歴正常とは言えない。mask の意味と未実施の時系列／DSC オフ／クリア後監視は[表示調査全文](../../docs/display-investigation.ja.md)を参照。クリア後監視には別コードが必要で、DscProbe に書き込みインターフェースはない。

ビルドはリポジトリ外で行うか、全生成物を追跡対象外の場所に置く。`sign.bat` は署名ユーザーの証明書ストア内の秘密鍵を使うので、例示 subject を自分のものに置き換える。電話には必要な公開証明書を登録し、秘密鍵も証明書ファイルもコミットしない。テスト署名とパッケージ署名は[導入](../../docs/install.ja.md)を参照。

プローブサービスを停止・削除しても GPU は無効にならない。別に graphics を無効化し、再起動前に ConfigFlags=1 を確認する。クラッシュでできなければオフライン復旧する。例の出力先は汎用のツール既定値で、作者の非公開ディレクトリ構成の再現を求めるものではない。
