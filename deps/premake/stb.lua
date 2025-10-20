stb = {
    source = path.join(dependencies.basePath, "stb"),
}

function stb.project()
end

function stb.import()
    stb.includes()
end

function stb.includes()
    includedirs {
        stb.source
    }
end

-- No project() or links—insert to dependencies for import calls only
table.insert(dependencies, stb)