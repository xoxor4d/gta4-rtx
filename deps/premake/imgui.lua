imgui = {
	source = path.join(dependencies.basePath, "imgui"),
}

function imgui.import()
	links { "imgui" }
	imgui.includes()
end

function imgui.includes()
	includedirs {
		imgui.source
	}
end

function imgui.project()
	project "imgui"
		language "C++"

		imgui.includes()

		files {
            path.join(imgui.source, "*.cpp"),
			path.join(imgui.source, "*.hpp"),
            path.join(imgui.source, "*.c"),
            path.join(imgui.source, "*.h"),
            path.join(imgui.source, "backends/imgui_impl_dx9.cpp"),
            path.join(imgui.source, "backends/imgui_impl_dx9.h"),
            path.join(imgui.source, "backends/imgui_impl_win32.cpp"),
            path.join(imgui.source, "backends/imgui_impl_win32.h"),
            path.join(imgui.source, "misc/cpp/imgui_stdlib.cpp"),
            path.join(imgui.source, "misc/cpp/imgui_stdlib.h"),
		}

		warnings "Off"
		kind "StaticLib"
end

table.insert(dependencies, imgui)
