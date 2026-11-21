#include "check.h"
#include "qol/VersionCompare.h"

using qol::isNewerVersion;

TEST(version_newer) {
    CHECK(isNewerVersion("0.5.0", "0.4.7"));
    CHECK(isNewerVersion("v0.5.1", "0.5.0"));
    CHECK(isNewerVersion("0.4.10", "0.4.9"));  // numeric, not text
    CHECK(isNewerVersion("1.0", "0.9.9"));
    CHECK(isNewerVersion("0.5.0.1", "0.5.0"));
}

TEST(version_not_newer) {
    CHECK(!isNewerVersion("0.5.1", "0.5.1"));
    CHECK(!isNewerVersion("v0.5.1", "0.5.1"));
    CHECK(!isNewerVersion("0.4.7", "0.5.0"));
    CHECK(!isNewerVersion("0.5", "0.5.0"));
}

TEST(version_beta_and_garbage) {
    CHECK(!isNewerVersion("0.5.1", "0.5.1-beta.3"));  // the numeric core is compared
    CHECK(isNewerVersion("0.5.2", "0.5.1-beta.3"));
    CHECK(!isNewerVersion("", "0.5.1"));
    CHECK(!isNewerVersion("latest", "0.5.1"));
    CHECK(!isNewerVersion("0.5.1", ""));
}
