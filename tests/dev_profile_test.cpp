// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// Explicit test profiles must isolate saves and cache without changing HOME.
#include <user/paths.h>
#include <cassert>
#include <cstdlib>

int main()
{
    namespace fs = std::filesystem;
    const fs::path normalData = GetUserPath(), normalCache = GetCachePath();
#ifdef _WIN32
    _putenv_s("NFSMW_USER_DIR", "dev-profile/data");
    _putenv_s("NFSMW_CACHE_DIR", "dev-profile/cache");
#else
    setenv("NFSMW_USER_DIR", "dev-profile/data", 1);
    setenv("NFSMW_CACHE_DIR", "dev-profile/cache", 1);
#endif
    assert(GetUserPath() == fs::absolute("dev-profile/data"));
    assert(GetSavePath(false) == GetUserPath() / "save");
    assert(GetCachePath() == fs::absolute("dev-profile/cache"));
    // Even if an old home profile exists, no home migration is attempted.
    assert(MigrateUserFolders().empty());
    assert(!fs::exists("dev-profile"));
#ifdef _WIN32
    _putenv_s("NFSMW_USER_DIR", "");
    _putenv_s("NFSMW_CACHE_DIR", "");
#else
    unsetenv("NFSMW_USER_DIR");
    unsetenv("NFSMW_CACHE_DIR");
#endif
    assert(GetUserPath() == normalData && GetCachePath() == normalCache);
}
