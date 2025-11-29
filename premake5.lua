dependencies = {
	basePath = "./deps"
}

function dependencies.load()
	dir = path.join(dependencies.basePath, "premake/*.lua")
	deps = os.matchfiles(dir)

	for i, dep in pairs(deps) do
		dep = dep:gsub(".lua", "")
		require(dep)
	end
end

function dependencies.imports()
	for i, proj in pairs(dependencies) do
		if type(i) == 'number' then
			proj.import()
		end
	end
end

function dependencies.projects()
	for i, proj in pairs(dependencies) do
		if type(i) == 'number' then
			proj.project()
		end
	end
end

dependencies.load()

workspace "gta4-rtx"

	startproject "gta4-rtx"
	location "./build"
	objdir "%{wks.location}/obj"
	targetdir "%{wks.location}/bin/%{cfg.buildcfg}"
	
    configurations { 
        "Debug", 
        "Release",
		"DebugSteam", 
        "ReleaseSteam",
    }

	platforms "Win32"
	architecture "x86"

	cppdialect "C++20"
	systemversion "latest"
    symbols "On"
    staticruntime "On"

    disablewarnings {
		"4239",
		"4369",
		"4505",
		"4996",
		"6001",
		"6385",
		"6386",
		"26812"
	}

    defines { 
        "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS" 
    }

    filter "platforms:Win*"
		defines {
			"_WINDOWS", 
			"WIN32"
		}
	filter {}

	-- Release

	filter "configurations:Release"
		optimize "Full"

		buildoptions {
			"/GL"
		}

		defines {
			"NDEBUG"
		}
		
		flags { 
            "MultiProcessorCompile", 
            "LinkTimeOptimization", 
            "No64BitChecks",
			"FatalCompileWarnings"
        }
	filter {}

	-- Debug

	filter "configurations:Debug"
		optimize "Debug"

		defines { 
            "DEBUG", 
            "_DEBUG" 
        }

		flags { 
            "MultiProcessorCompile", 
            "No64BitChecks" 
        }
	filter {}

	local is_steam_build = false

	filter "configurations:DebugSteam"
		optimize "Debug"
		
		is_steam_build = true

		defines { 
			"DEBUG", 
			"_DEBUG"
		}
	
		flags { 
			"MultiProcessorCompile", 
			"No64BitChecks" 
		}
	filter {}

	filter "configurations:ReleaseSteam"
		is_steam_build = true
	filter {}

	-- Projects

	project "_shared"
		kind "StaticLib"
		language "C++"

		targetdir "bin/%{cfg.buildcfg}"
		objdir "obj/%{cfg.buildcfg}"
		
		pchheader "std_include.hpp"
		pchsource "src/shared/std_include.cpp"

		files {
			"./src/shared/**.hpp",
			"./src/shared/**.cpp",
		}

		includedirs {
			"%{prj.location}/src",
			"./src",
		}

		resincludedirs {
			"$(ProjectDir)src"
		}

        buildoptions { 
            "/Zm100 -Zm100" 
        }

        -- Specific configurations
		flags { 
			"UndefinedIdentifiers" 
		}

		warnings "Extra"
		dependencies.imports()

        group "Dependencies"
            dependencies.projects()
		group ""

	---------------------------

	project "gta4-rtx"
	kind "SharedLib"
	language "C++"

	linkoptions {
		"/PDBCompress"
	}

	pchheader "std_include.hpp"
	pchsource "src/gta4/std_include.cpp"

	files {
		"./src/gta4/**.hpp",
		"./src/gta4/**.cpp",
	}

	includedirs {
		"%{prj.location}/src",
		"./src",
	}

	links {
		"_shared"
	}

	resincludedirs {
		"$(ProjectDir)src"
	}

	buildoptions { 
		"/Zm100 -Zm100" 
	}

	filter "configurations:Debug or configurations:Release"
		if(os.getenv("GTA4_ROOT")) then
			print ("Setup paths using environment variable 'GTA4_ROOT' :: '" .. os.getenv("GTA4_ROOT") .. "'")
			targetdir(os.getenv("GTA4_ROOT"))
			debugdir (os.getenv("GTA4_ROOT"))
			debugcommand (os.getenv("GTA4_ROOT") .. "/" .. "GTAIV.exe")
		end
	filter {}

	filter "configurations:DebugSteam or configurations:ReleaseSteam"
		if(os.getenv("GTA4_STEAM_ROOT")) then
			print ("Setup paths using environment variable 'GTA4_STEAM_ROOT' :: '" .. os.getenv("GTA4_STEAM_ROOT") .. "'")
			targetdir(os.getenv("GTA4_STEAM_ROOT"))
			debugdir (os.getenv("GTA4_STEAM_ROOT"))
			debugcommand (os.getenv("GTA4_STEAM_ROOT") .. "/" .. "GTAIV.exe")
		end
	filter {}

	-- Specific configurations
	flags { 
		"UndefinedIdentifiers" 
	}

	warnings "Extra"

	-- Post-build
	postbuildcommands {
		"MOVE /Y \"$(TargetDir)gta4-rtx.dll\" \"$(TargetDir)a_gta4-rtx.asi\"",
	}

	dependencies.imports()

	group "Dependencies"
		dependencies.projects()
	group ""
	

project "installer"
    kind "ConsoleApp"
	targetname "GTAIV-Remix-CompMod-Installer"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"
    targetdir "./bin"
    

	files {
		"./src_installer/**.hpp",
		"./src_installer/**.cpp",
		"./deps/miniz/miniz.c",
		"./deps/miniz/miniz.h",
		"./src_installer/installer.rc",
		"./src_installer/installer.manifest"
	}

	includedirs {
		"%{prj.location}/src_installer",
		"./src_installer",
		"./deps/miniz",
	}

    filter "configurations:Release*"
        optimize "Full"
        flags { "LinkTimeOptimization" }
	filter {}
	
	-- Disable automatic manifest generation (we embed it via resource file)
	linkoptions {
		"/MANIFEST:NO"
	}

	dependencies.imports()

	group "Dependencies"
		dependencies.projects()
	group ""