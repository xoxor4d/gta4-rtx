toml11 = {
	source = path.join(dependencies.basePath, "toml11"),
}

function toml11.import()
	dependson "toml11"
	links { "toml11" }
	toml11.includes()
end

function toml11.includes()
	includedirs {
		path.join(toml11.source, "include"),
	}
end

function toml11.project()
	project "toml11"
		language "C"

		toml11.includes()
		files
		{
			path.join(toml11.source, "include/***")
		}

		warnings "Off"
		kind "None"
end

table.insert(dependencies, toml11)
