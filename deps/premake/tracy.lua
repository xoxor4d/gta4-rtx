tracy = {
	source = path.join(dependencies.basePath, "tracy/public"),

	-- the client library and every project including its headers have to agree on these
	defines = {
		"TRACY_ENABLE",
		"TRACY_ON_DEMAND",
		"TRACY_DELAYED_INIT",
		"TRACY_NO_VSYNC_CAPTURE",
	},
}

function tracy.import()
	filter "configurations:*Tracy"
		dependson "tracy"
		links { "tracy" }
		defines { "GTA4_PROFILING" }
		defines(tracy.defines)
		tracy.includes()
	filter {}
end

function tracy.includes()
	includedirs {
		tracy.source
	}
end

function tracy.project()
	project "tracy"
		language "C++"

		tracy.includes()

		filter "configurations:*Tracy"
			defines(tracy.defines)
		filter {}

		files {
			path.join(tracy.source, "TracyClient.cpp"),
			path.join(tracy.source, "tracy/*.hpp"),
			path.join(tracy.source, "client/*.hpp"),
			path.join(tracy.source, "common/*.hpp"),
		}

		warnings "Off"
		kind "StaticLib"
end

table.insert(dependencies, tracy)
