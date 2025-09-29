#include "std_include.hpp"
#include "remix.hpp"

namespace shared::common::remix
{
	void set_texture_category(IDirect3DDevice9* dev, const std::uint32_t& cat)
    {
    	dev->SetRenderState((D3DRENDERSTATETYPE)42, cat);
    }

    void reset_texture_category(IDirect3DDevice9* dev)
    {
        dev->SetRenderState((D3DRENDERSTATETYPE)42, 0xfefefefe);
    }

	void set_texture_hash(IDirect3DDevice9* dev, const std::uint32_t& hash)
	{
		dev->SetRenderState((D3DRENDERSTATETYPE)150, hash);
	}

	void reset_texture_hash(IDirect3DDevice9* dev)
	{
		dev->SetRenderState((D3DRENDERSTATETYPE)150, 0xfefefefe);
	}
}
