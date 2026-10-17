--     Copyright (C) 2026 Valtteri Viirret
--     This file is part of the Nexilis Project.
--
--     This file is free software: you can redistribute it and/or modify
--     it under the terms of the GNU Lesser General Public License as
--     published by the Free Software Foundation, either version 3 of the
--     License, or (at your option) any later version.
--
--     This file is distributed in the hope that it will be useful,
--     but WITHOUT ANY WARRANTY; without even the implied warranty of
--     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--     GNU Lesser General Public License for more details.
--
--     You should have received a copy of the GNU Lesser General Public License
--     along with this file.  If not, see <https://gnu.org>.

workspace("NexilisTest")
configurations({ "Debug" })
architecture("x64")

project("premake_test")
kind("ConsoleApp")
language("C++")
cppdialect("C++20")
files({ "premake_test.cc" })
targetdir("bin/%{cfg.buildcfg}")
objdir("obj/%{cfg.buildcfg}")
linkoptions({
	"-L../../nexilis/bin/Debug/",
	"-l:libnexilis-premake_d.so",
	-- The client headers pull in header-only boost.asio TLS code that
	-- references OpenSSL symbols.
	"-lcrypto",
	"-lssl",
	-- Find libnexilis-premake_d.so next to the executable.
	"-Wl,-rpath,'$$ORIGIN'",
})
local boost_includes = os.matchdirs("../../nexilis/third-party/boost/libs/*/include")
includedirs({
	"../../nexilis/include",
	boost_includes,
})

postbuildcommands({
	"cp ../../nexilis/bin/Debug/libnexilis-premake_d.so bin/Debug/",
	"chmod +x bin/Debug/libnexilis-premake_d.so",
	"echo 'Library ready in bin/Debug/'",
})
