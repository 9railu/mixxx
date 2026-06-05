# SQLCipher × SQLite3 シンボル競合

**発生箇所：** Rekordbox DB インポート
**発生日：** 2026-06-05
**状態：** 解決済み

---

## 問題の内容

SQLCipher と SQLite3 は `sqlite3_*` シンボルを完全に共有しており、
両方を同一バイナリにリンクすると以下のリンクエラーが発生する。

```
sqlite3_version already defined in sqlite3.lib
one or more multiply defined symbols found
```

Mixxx は mixxxdb.sqlite の読み書きに QSqlDatabase（内部で SQLite3）を使用しているため、
SQLCipher を追加すると競合する。

---

## 解決内容

**qsqlcipher-qt6 プラグインで Qt の QSQLITE ドライバーを差し替える。**

- GitHub: `chehrlic/qsqlcipher-qt6`
- SQLCipher は SQLite3 の完全上位互換のため：
  - mixxxdb.sqlite（暗号化なし）→ そのまま開ける
  - master.db（Rekordbox・暗号化あり）→ 復号キーを PRAGMA key で指定して開ける

**BUILD_SETUP.md Step 4 に手順を追記済み。**
**RISKS.md に記録済み。**
