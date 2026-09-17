// selftest - exercises the whole licensing core against a temp database.
//
// Covers key issuing, offline signature verification, forgery rejection,
// account creation + binding, duration expiry, use consumption/exhaustion,
// revocation and the double-bind guard. Returns non-zero on any failure.
#include "nglicense/Crypto.h"
#include "nglicense/Database.h"
#include "nglicense/License.h"
#include "nglicense/LicenseService.h"

#include <cstdio>
#include <string>

using namespace ngl;

namespace {
int g_failures = 0;

void check(bool condition, const char *label)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        ++g_failures;
}
} // namespace

int main()
{
    KeyPair issuer = crypto::generateKeyPair();

    Database db;
    std::string error;
    // A private temp db keeps the test hermetic.
    if (!db.open(":memory:", &error)) {
        std::printf("cannot open db: %s\n", error.c_str());
        return 2;
    }

    LicenseService service(db, issuer.publicKey);
    service.setIssuerSeed(issuer.privateSeed);

    std::printf("Issuing + offline verification\n");
    IssueSpec durationSpec;
    durationSpec.type = LicenseType::Duration;
    durationSpec.durationSeconds = 30 * 86400;
    IssueResult issued = service.issue(durationSpec);
    check(issued.ok, "issue duration licence");
    check(license::parseAndVerify(issuer.publicKey, issued.keyString).has_value(),
          "genuine key verifies offline");

    KeyPair attacker = crypto::generateKeyPair();
    check(!license::parseAndVerify(attacker.publicKey, issued.keyString).has_value(),
          "forged issuer cannot pass verification");

    std::printf("Account creation + binding\n");
    ValidationResult create =
        service.createAccount("darcy", "d@example.com", "hunter2024", issued.keyString, "PC-1");
    check(create.ok && create.licensed, "create account with valid key -> licensed");

    ValidationResult dupe =
        service.createAccount("darcy2", "d2@example.com", "hunter2024", issued.keyString, "PC-2");
    check(!dupe.ok, "same key cannot bind a second account");

    ValidationResult signin = service.signIn("darcy", "hunter2024", "", "PC-1", false);
    check(signin.ok && signin.licensed, "sign in -> authenticated + licensed");

    ValidationResult wrongPw = service.signIn("darcy", "nope", "", "PC-1", false);
    check(!wrongPw.ok, "wrong password rejected");

    std::printf("Freemium: account without a licence, then redeem\n");
    ValidationResult free =
        service.createAccount("freeuser", "", "password1", "", "PC-9");
    check(free.ok && !free.licensed, "create account WITHOUT a key -> in but locked");

    ValidationResult freeSignin = service.signIn("freeuser", "password1", "", "PC-9", false);
    check(freeSignin.ok && !freeSignin.licensed, "unlicensed sign in -> in but locked");

    IssueSpec freeGrant;
    freeGrant.type = LicenseType::Duration;
    freeGrant.durationSeconds = 30 * 86400;
    IssueResult freeKey = service.issue(freeGrant);
    ValidationResult redeemed =
        service.redeemLicense(freeSignin.accountId, freeKey.keyString, "PC-9");
    check(redeemed.ok && redeemed.licensed, "redeem a key -> unlocked");

    ValidationResult afterRedeem = service.signIn("freeuser", "password1", "", "PC-9", false);
    check(afterRedeem.ok && afterRedeem.licensed, "sign in after redeem -> licensed");

    std::printf("Use-limited licences\n");
    IssueSpec usesSpec;
    usesSpec.type = LicenseType::Uses;
    usesSpec.maxUses = 2;
    IssueResult usesKey = service.issue(usesSpec);
    ValidationResult acc2 =
        service.createAccount("gamer", "", "password1", usesKey.keyString, "PC-3");
    check(acc2.ok, "create use-limited account");

    ValidationResult use1 = service.signIn("gamer", "password1", "", "PC-3", true);
    ValidationResult use2 = service.signIn("gamer", "password1", "", "PC-3", true);
    ValidationResult use3 = service.signIn("gamer", "password1", "", "PC-3", true);
    check(use1.licensed && use1.usesRemaining == 1, "first use leaves 1 remaining");
    check(use2.licensed && use2.usesRemaining == 0, "second use leaves 0 remaining");
    check(use3.ok && !use3.licensed, "exhausted -> signed in but locked");

    std::printf("Revocation\n");
    check(service.revoke(issued.keyId, &error), "revoke the duration licence");
    ValidationResult afterRevoke = service.signIn("darcy", "hunter2024", "", "PC-1", false);
    check(afterRevoke.ok && !afterRevoke.licensed, "revoked -> signed in but locked");

    std::printf("Expiry\n");
    IssueSpec expiredSpec;
    expiredSpec.type = LicenseType::Duration;
    expiredSpec.durationSeconds = 1;
    IssueResult shortKey = service.issue(expiredSpec);
    // Force it into the past directly so the test does not have to sleep.
    db.execute("UPDATE licenses SET expiry_unix=? WHERE key_id=?",
               {nowUnix() - 10, shortKey.keyId});
    ValidationResult expiredCreate =
        service.createAccount("late", "", "password1", shortKey.keyString, "PC-4");
    check(!expiredCreate.ok, "expired licence cannot be redeemed");

    std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
