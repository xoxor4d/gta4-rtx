bridge_api = {
	source = path.join(dependencies.basePath, "bridge_api"),
}

function bridge_api.import()
	links { "bridge_api" }
	bridge_api.includes()
end

function bridge_api.includes()
	includedirs {
		bridge_api.source
	}
end

function bridge_api.project()
	project "bridge_api"
		language "C++"

		bridge_api.includes()

		files {
            path.join(bridge_api.source, "**"),
		}

		warnings "Off"
		kind "None"
end

table.insert(dependencies, bridge_api)
