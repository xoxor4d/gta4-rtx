dxsdk = {
	source = path.join(dependencies.basePath, "dxsdk"),
}

function dxsdk.import()
	filter "platforms:*32"
		libdirs { path.join(dxsdk.source, "Lib/x86") }
		
	filter "platforms:*64"
		libdirs { path.join(dxsdk.source, "Lib/x64") }
		
	filter {}
	
	dxsdk.includes()
end

function dxsdk.includes()
	includedirs {
		path.join(dxsdk.source, "Include")
	}
end

function dxsdk.project()
    project "dxsdk"
		language "C++"

		dxsdk.includes()

		files {
            path.join(dxsdk.source, "Include/*.h"),
			path.join(dxsdk.source, "Include/*.inl"),
		}

		warnings "Off"
		kind "None"

end

table.insert(dependencies, dxsdk)
