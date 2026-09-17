#include "nglicense/Database.h"

#include <sqlite3.h>

namespace ngl {

std::int64_t Row::integer(int column) const
{
    return sqlite3_column_int64(m_stmt, column);
}

std::string Row::text(int column) const
{
    const unsigned char *value = sqlite3_column_text(m_stmt, column);
    if (!value)
        return {};
    return reinterpret_cast<const char *>(value);
}

bool Row::isNull(int column) const
{
    return sqlite3_column_type(m_stmt, column) == SQLITE_NULL;
}

Database::~Database()
{
    close();
}

bool Database::open(const std::string &path, std::string *error)
{
    close();

    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        m_lastError = m_db ? sqlite3_errmsg(m_db) : "could not allocate database";
        if (error)
            *error = m_lastError;
        close();
        return false;
    }

    // Durability and concurrency defaults suited to a small always-on server.
    sqlite3_busy_timeout(m_db, 5000);
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");

    return applySchema(error);
}

void Database::close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

namespace {

bool bindParams(sqlite3_stmt *stmt, const std::vector<SqlValue> &params, std::string *error)
{
    for (std::size_t i = 0; i < params.size(); ++i) {
        const int index = static_cast<int>(i) + 1;
        const SqlValue &value = params[i];
        int rc = SQLITE_OK;
        switch (value.kind) {
        case SqlValue::Kind::Null:
            rc = sqlite3_bind_null(stmt, index);
            break;
        case SqlValue::Kind::Int:
            rc = sqlite3_bind_int64(stmt, index, value.integer);
            break;
        case SqlValue::Kind::Text:
            rc = sqlite3_bind_text(stmt, index, value.text.c_str(),
                                   static_cast<int>(value.text.size()), SQLITE_TRANSIENT);
            break;
        }
        if (rc != SQLITE_OK) {
            if (error)
                *error = "bind failed";
            return false;
        }
    }
    return true;
}

} // namespace

bool Database::execute(const std::string &sql, const std::vector<SqlValue> &params,
                       std::string *error)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        if (error)
            *error = m_lastError;
        return false;
    }

    bool ok = bindParams(stmt, params, error);
    if (ok) {
        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            m_lastError = sqlite3_errmsg(m_db);
            if (error)
                *error = m_lastError;
            ok = false;
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::query(const std::string &sql, const std::vector<SqlValue> &params,
                     const std::function<void(const Row &)> &onRow, std::string *error)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        if (error)
            *error = m_lastError;
        return false;
    }

    if (!bindParams(stmt, params, error)) {
        sqlite3_finalize(stmt);
        return false;
    }

    Row row(stmt);
    int rc = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (onRow)
            onRow(row);
    }

    const bool ok = (rc == SQLITE_DONE);
    if (!ok) {
        m_lastError = sqlite3_errmsg(m_db);
        if (error)
            *error = m_lastError;
    }
    sqlite3_finalize(stmt);
    return ok;
}

std::int64_t Database::lastInsertId() const
{
    return sqlite3_last_insert_rowid(m_db);
}

int Database::changes() const
{
    return sqlite3_changes(m_db);
}

bool Database::applySchema(std::string *error)
{
    static const char *kSchema =
        "CREATE TABLE IF NOT EXISTS meta ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS licenses ("
        "  key_id          TEXT PRIMARY KEY,"
        "  type            INTEGER NOT NULL,"
        "  issued_unix     INTEGER NOT NULL,"
        "  expiry_unix     INTEGER NOT NULL DEFAULT 0,"
        "  max_uses        INTEGER NOT NULL DEFAULT 0,"
        "  uses_consumed   INTEGER NOT NULL DEFAULT 0,"
        "  status          INTEGER NOT NULL DEFAULT 0," // 0 active, 1 revoked
        "  issued_by       TEXT,"
        "  discord_user_id TEXT,"
        "  notes           TEXT,"
        "  bound_account_id INTEGER,"
        "  bound_machine   TEXT,"
        "  key_string      TEXT,"
        "  created_unix    INTEGER NOT NULL);"

        "CREATE TABLE IF NOT EXISTS accounts ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username      TEXT NOT NULL UNIQUE COLLATE NOCASE,"
        "  email         TEXT UNIQUE COLLATE NOCASE,"
        "  pass_hash     TEXT NOT NULL,"
        "  pass_salt     TEXT NOT NULL,"
        "  iterations    INTEGER NOT NULL,"
        "  license_key_id TEXT,"
        "  created_unix  INTEGER NOT NULL);"

        "CREATE TABLE IF NOT EXISTS activations ("
        "  id             INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  key_id         TEXT NOT NULL,"
        "  account_id     INTEGER,"
        "  machine_id     TEXT,"
        "  activated_unix INTEGER NOT NULL,"
        "  last_seen_unix INTEGER NOT NULL);"

        "CREATE INDEX IF NOT EXISTS idx_activations_key ON activations(key_id);"

        "CREATE TABLE IF NOT EXISTS blacklist ("
        "  value        TEXT NOT NULL,"   // key id or machine id
        "  kind         TEXT NOT NULL,"   // 'license' or 'machine'
        "  reason       TEXT,"
        "  created_by   TEXT,"
        "  created_unix INTEGER NOT NULL,"
        "  PRIMARY KEY (value, kind));";

    char *errMsg = nullptr;
    if (sqlite3_exec(m_db, kSchema, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = errMsg ? errMsg : "schema creation failed";
        if (error)
            *error = m_lastError;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

} // namespace ngl
