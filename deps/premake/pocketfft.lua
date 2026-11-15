pocketfft = {
	source = path.join(dependencies.basePath, "pocketfft"),
}

function pocketfft.import()
	pocketfft.includes()
end

function pocketfft.includes()
	includedirs {
		pocketfft.source,
	}
end

function pocketfft.project()

end

table.insert(dependencies, pocketfft)
