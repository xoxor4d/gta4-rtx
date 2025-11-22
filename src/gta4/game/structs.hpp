#pragma once

namespace gta4::game
{
	struct geometry
	{
		void* vtbl;
		int vertexdecl;
		int pad1;
		int vb[4];
		int ib[4];
		int ibCount;
		int faceCount;
		uint16_t vertCount;
		uint16_t indicesPerFace;
		int pad2;
		uint16_t stride;
		uint16_t pad3;
		int pUnk1;
		int pUnk2;
		int pad4;
	};

	struct geometry_ptr
	{
		geometry* geo;
	};

	struct model
	{
		void* vtbl;
		geometry_ptr* m_geometries;
		uint16_t m_geocount;
		uint16_t m_geosize;
		int pUnk1;
		int pShaderRelated;
		char cUnk0;
		bool isSkinned;
		char cUnk1;
		char cUnk2;
		char cUnk3;
		char cUnk4;
		uint16_t shaderRelatedCount;
	};

	struct drawableReference
	{
		int pDrawable;
		int pShaderEffect;
		int field_8;
		int pPrev;
		int pNext;
	};

	// from https://public.sannybuilder.com/gtasa_exe_idb/gta_iv_eflc/
	struct CEntity
	{
		void* vtbl;
		int field_4;
		int field_8;
		int field_C;
		Vector m_pos;
		int m_fHeading;                       ///< float
		D3DXMATRIX* worldTransform;                        ///< pointer to position matrix
		int m_dwFlags;                        ///< see CFlags
		///< 0x1 = UsesCollision
		///< 0x4 = Fixed (frozen)
		///< 0x8 = FixedWaitingForCollision
		///< 0x10 = FixedByPhysics
		///< 0x20 = Visible
		///< 0x100 = Damaged
		///< 0x1000 = draw last
		///< 0x4000000 = Has Physics Inst
		int m_dwFlags2;                       ///< flags - 2 - lights, 21 - onFire
		///< 0x40 thru 0x200 (4 bits)
		///< --> & 0x3C0
		///< --> object type (2 = playerPed, 4 = ped)
		///< 0x200000 = OnFire
		///< 0x400000 = have coords matrix
		__int16 field_2C;
		__int16 m_wModelIndex;
		int m_pReference;
		drawableReference* m_pDrawableReference;
		int m_pPhysics;                       ///< phInst *
		int field_3C;
		char field_40;
		char field_41;
		__int16 field_42;
		__int16 field_44;
		__int16 field_46;
		int m_hInterior;
		int field_4C;
		int field_50;                         ///< float
		int field_54;
		int field_58;
		__int16 m_wIplIndex;
		__int16 field_5E;
		char field_60;
		char field_61;
		char field_62;
		char m_alpha;
		int field_64;
		int field_68;
		int m_pNetEntity;
	};

	struct cmdarg
	{
		const char* arg_name;
		const char* arg_desc;
		const char* arg_val;
	};

	// (AssaultKifle47) https://github.com/akifle47/InGameTimecycEditor/blob/main/source/TimeCycle.h
	struct TimeCycleParams
	{
		//unknown means its a timecyc.dat parameter and i dont know what it does
		//unused means its not a timecyc.dat parameter and i dont care what it does
		uint32_t mAmbient0Color;
		uint32_t mAmbient1Color;
		uint32_t mDirLightColor;
		float mDirLightMultiplier;
		float mAmbient0Multiplier;
		float mAmbient1Multiplier;
		float mAOStrength;
		float mPedAOStrength;
		float mRimLightingMultiplier;
		float mSkyLightMultiplier;
		float mDirLightSpecMultiplier;
		uint32_t mSkyBottomColorFogDensity;
		uint32_t mSunCore;
		float mCoronaBrightness;
		float mCoronaSize;
		float mDistantCoronaBrightness;
		float mDistantCoronaSize;
		float mFarClip;
		float mFogStart;
		float mDOFStart;
		float mNearDOFBlur;
		float mFarDOFBlur;
		uint32_t mLowCloudsColor;
		uint32_t mBottomCloudsColor;
		uint32_t mWater;
		float mUnused64[7];
		float mWaterReflectionMultiplier;
		float mParticleBrightness;
		float mExposure;
		float mBloomThreshold;
		float mMidGrayValue;
		float mBloomIntensity;
		uint32_t mColorCorrection;
		uint32_t mColorAdd;
		float mDesaturation;
		float mContrast;
		float mGamma;
		float mDesaturationFar;
		float mContrastFar;
		float mGammaFar;
		float mDepthFxNear;
		float mDepthFxFar;
		float mLumMin;
		float mLumMax;
		float mLumDelay;
		int32_t mCloudAlpha;
		float mUnusedD0;
		float mTemperature;
		float mGlobalReflectionMultiplier;
		float mUnusedDC;
		float mSkyColor[3];
		float mUnusedEC;
		float mSkyHorizonColor[3];
		float mUnusedFC;
		float mSkyEastHorizonColor[3];
		float mUnused10C;
		float mCloud1Color[3];
		float mUnknown11C;
		float mSkyHorizonHeight;
		float mSkyHorizonBrightness;
		float mSunAxisX;
		float mSunAxisY;
		float mCloud2Color[3];
		float mUnused13C;
		float mCloud2ShadowStrength;
		float mCloud2Threshold;
		float mCloud2Bias1;
		float mCloud2Scale;
		float mCloudInScatteringRange;
		float mCloud2Bias2;
		float mDetailNoiseScale;
		float mDetailNoiseMultiplier;
		float mCloud2Offset;
		float mCloudWarp;
		float mCloudsFadeOut;
		float mCloud1Bias;
		float mCloud1Detail;
		float mCloud1Threshold;
		float mCloud1Height;
		float mUnused17C;
		float mCloud3Color[3];
		float mUnused18C;
		float mUnknown190;
		float mUnused198[3];
		float mSunColor[3];
		float mUnused1AC;
		float mCloudsBrightness;
		float mDetailNoiseOffset;
		float mStarsBrightness;
		float mVisibleStars;
		float mMoonBrightness;
		float mUnused1C4[3];
		float mMoonColor[3];
		float mUnused1DC;
		float mMoonGlow;
		float mMoonParam3;
		float SunCenterStart;
		float SunCenterEnd;
		float mSunSize;
		float mUnused1F8[3];
		float mUnknown200;
		float mSkyBrightness;
		float mUnused208;
		int32_t mFilmGrain;
	};


	// https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix

	enum eWeatherType : uint32_t
	{
		WEATHER_EXTRASUNNY,
		WEATHER_SUNNY,
		WEATHER_SUNNY_WINDY,
		WEATHER_CLOUDY,
		WEATHER_RAIN,
		WEATHER_DRIZZLE,
		WEATHER_FOGGY,
		WEATHER_LIGHTNING,
		WEATHER_NONE
	};

	inline const char* eWeatherTypeStr[] =
	{
		"WEATHER_EXTRASUNNY",
		"WEATHER_SUNNY",
		"WEATHER_SUNNY_WINDY",
		"WEATHER_CLOUDY",
		"WEATHER_RAIN",
		"WEATHER_DRIZZLE",
		"WEATHER_FOGGY",
		"WEATHER_LIGHTNING",
		"WEATHER_NONE"
	};

	struct grmShaderInfo_Parameter
	{
		char nbType;
		char nbCount;
		char nbValueLength;
		char nbAnnotationsCount;
		const char* pszName;
		const char* pszDescription;
		int dwNameHash;
		int dwDescriptionHash;
		int pAnnotations;
		void* pValue;
		int16_t m_wVertexFragmentRegister;
		int16_t m_wPixelFragmentRegister;
		int pdwParameterHashes;
		int field_24;
		int field_28;
		int field_2C;
	};

	enum eLightType
	{
		LT_POINT = 0x0,
		LT_DIR = 0x1,
		LT_SPOT = 0x2,
		LT_AO = 0x3,
		LT_CLAMPED = 0x4,
	};

	struct CLightSource
	{
		Vector mDirection;
		float field_C;
		Vector mTangent;
		float field_1C;
		Vector mPosition;
		float field_2C;
		Vector4D mColor;
		float mIntensity;
		eLightType mType;
		int mFlags;
		int mTxdHash;
		int mTextureHash;
		float mRadius;
		float mInnerConeAngle;
		float mOuterConeAngle;
		int field_60;
		int32_t mShadowCacheIndex;
		int mInteriorIndex;
		int mRoomIndex;
		float mVolumeSize;
		float mVolumeScale;
		int8_t gap78[7];
		char field_7F;
	};

	struct CDirectionalLight
	{
		Vector mDirection;
		float field_C;
		int8_t gap10[32];
		Vector4D mColor;
		float mIntensity;
		int field_44;
		int8_t gap48[56];
	};

	// ------



	struct sceneviewport_s
	{
		char pad_0x0000[0x10]; //0x0000
		D3DXMATRIX N0000044C; //0x0010 
		D3DXMATRIX cameraInv; //0x0050 
		D3DXMATRIX identity; //0x0090 
		D3DXMATRIX wv; //0x00D0 
		D3DXMATRIX wvp; //0x0110 
		D3DXMATRIX viewInv; //0x0150 
		D3DXMATRIX view; //0x0190 
		D3DXMATRIX proj; //0x01D0 
		char pad_0x0210[0x5F4]; //0x0210
	};

	struct g_viewports2
	{
		char pad_0x0000[0x10]; //0x0000
		sceneviewport_s* sceneviewport; //0x0010 
		char pad_0x0014[0x2C]; //0x0014
	}; //Size=0x0040


	struct grcViewport
	{
		D3DXMATRIX world;
		D3DXMATRIX cameraInv;
		D3DXMATRIX N00000917;
		D3DXMATRIX wv;
		D3DXMATRIX wvp;
		D3DXMATRIX viewInv;
		D3DXMATRIX view;
		D3DXMATRIX proj;
		D3DXMATRIX wvpFrustum;
		D3DXMATRIX localFrustum;
		float wp1_x;
		float wp1_y;
		float wp1_w;
		float wp1_h;
		float wp1_minZ;
		float wp1_maxZ;
		float wp2_x;
		float wp2_y;
		float wp2_w;
		float wp2_h;
		float wp2_minZ;
		float wp2_maxZ;
		int width;
		int height;
		float fov;
		float aspect;
		float nearclip;
		float farclip;
		char pad_0x02C8[8];
		float scalex;
		float scaley;
		char pad_0x02D8[24];
		bool isPersp;
		char pad_0x02F1[15];
		float frustumClipPlane0[4];
		float frustumClipPlane1[4];
		float frustumClipPlane2[4];
		float frustumClipPlane3[4];
		float frustumClipPlane4[4];
		float frustumClipPlane5[4];
		char pad_0x0360[420];
	};

	struct CRenderphase
	{
		DWORD dword0;
		BYTE gap4[24];
		BYTE byte1C;
		BYTE gap1D[147];
		grcViewport grcviewportB0;
		grcViewport grcviewportB1;
		BYTE ga[100];
		int unk;
	};

	struct currentViewport_ptr {
		grcViewport* wp;
	};

	struct constant_register_stack_s
	{
		std::uint8_t type;
		__int8 pad;
		std::uint16_t register_num;
		int unk;
	};

	struct ps_data_s
	{
		int unk0;
		constant_register_stack_s* register_pool;
		IDirect3DPixelShader9* shader;
	};

	struct vs_data_s
	{
		int unk0;
		constant_register_stack_s* register_pool;
		IDirect3DVertexShader9* shader;
	};

	struct const_stack_s
	{
		std::uint8_t constant_pool_index;
		std::uint8_t register_pool_index;
	};

	struct vs_info_s
	{
		int vs_data_index;
		const_stack_s* vs_constant_stack;
		__int16 num_vs_constants;
		__int16 unk;
	};

	struct ps_info_s
	{
		int ps_data_index;
		const_stack_s* ps_constant_stack;
		__int16 num_ps_constants;
		__int16 unk;
	};

	struct renderstate_value_stack_s
	{
		int renderstate_index;
		int renderstate_data;
	};

	struct current_pass_s
	{
		vs_info_s vs_info;
		ps_info_s ps_info;
		renderstate_value_stack_s* renderstate_value_stack;
		unsigned __int16 num_renderstates;
		__int16 pad;
	};

	


	struct grcTexturePC_vtbl
	{
		void* unk1;
		void* unk2;
		void* unk3;
		void* unk4;
		void* unk5;
		void* unk6;
		void* unk7;
		void* unk8;
		void* unk9;
		void* unk10;
	};

	struct grcTexturePC
	{
		grcTexturePC_vtbl* vtbl; //0x0000 
		char pad_0x0004[0x4]; //0x0004
		DWORD N000000D4; //0x0008 
		DWORD N000000D5; //0x000C 
		char pad_0x0010[0x4]; //0x0010
		char* texture_filename; //0x0014 
		IDirect3DBaseTexture9* d3d_texture_handle; //0x0018 
		char pad_0x001C[0x4]; //0x001C
		char fileformat_str[4]; //0x695888 
		char pad_0x0024[0x2C]; //0x0024
	}; //Size=0x0050

	struct grcTextureReference_vtbl
	{
		void* unk1;
		void* unk2;
		void* unk3;
		void* unk4;
		void* unk5;
		void* unk6;
		void* unk7;
		void* unk8;
		void* unk9;
		void* unk10;
	};

	struct grcTextureReference
	{
		grcTextureReference_vtbl* vtbl;
		char pad_0x0004[0x4]; //0x0004
		__int16 N00000096; //0x0008 
		__int16 N000000CD; //0x000A 
		char pad_0x000C[0x8]; //0x000C
		char* texture_name_no_ext; //0x0014 
		grcTexturePC* texture_info; //0x0018 
		char pad_0x001C[0x44]; //0x001C

	};

	// should be union?
	// also points to float arrays?
	union shader_constant_s
	{
		grcTextureReference* texture_ref;
		float* float_arr;
		int* int_ptr;
		BOOL* bool_ptr;
		void* constant_ptr;
	};

	struct sampler_constant_data_s
	{
		__int8 unk1_lo;
		__int8 unk2_hi;
		__int8 unk3_lo;
		__int8 unk3_hi;
		int unk4;
		int unk5;
		int unk6;
		int unk7;
		int unk8;
		int unk9;
		int unk10;
		int unk11;
		int unk12;
		int unk13;
		int unk14;
	};

	struct shader_data_sub_s
	{
		sampler_constant_data_s* sampler_constant_data;
		int pad1;
		vs_data_s* vs_data;
		int pad2;
		ps_data_s* ps_data;
		int pad3;
		const char* shader_name;
	};

	struct shader_data_s
	{
		void* phase_info;
		int pad0;
		shader_data_sub_s sub;
	};

	struct shader_info_sub_s
	{
		shader_constant_s* constants;
		shader_data_s* data;
		int N0000076E;
		int N0000076F;
		__int8* constant_float_count_array;
		char pad_0x0014[0x1C];
		const char* shader_name;
		const char* preset_name;
		int pad4;
		int pad5;
		int preset_index;
	};

	struct sampler_data_s
	{
		__int16 unk01;
		__int8 unk02;
		__int8 unk03;
		int unk2;
		int unk3;
		int unk4;
		int unk5;
		int unk6;
	};

	struct shaderfx_base
	{
		void* grmShaderFx_vtbl; //0x0000 
		int unk32_01; //0x0004 
		std::int8_t unk8_1; //0x0008 
		std::int8_t unk8_2; //0x0009 
		std::int8_t unk8_3; //0x000A 
		std::int8_t unk8_4; //0x000B 
		std::uint16_t unk16_01; //0x000C 
		std::uint16_t unk16_02; //0x000E 
		int unk32_02; //0x0010 
		game::shader_info_sub_s* sinfo_substruct; //0x0014 
		game::shader_data_s* sinfo_substruct_data; //0x0018 
		int unk32_03; //0x001C 
		int unk32_04; //0x0020 
		void* ptr01; //0x0024 
		DWORD maybe_shaderhash; //0x0028 
		int unk32_05; //0x002C 
		int unk32_06; //0x0030 
		void* ptr02; //0x0034 
		int unk32_07; //0x0038 
		int unk32_08; //0x003C 
		void* ptr03; //0x0040 
		char* shadername; //0x0044 
		char* spsname; //0x0048 
		int ret_grmShaderFx_m5; //0x004C 
		int ret_grmShaderFx_m6; //0x0050 
		int spsIndex; //0x0054 
		int ret_grmShaderFx_m15; //0x0058 
		char pad_0x005C[0x4]; //0x005C
		void* N00000513; //0x0060 
		char pad_0x0064[0x4]; //0x0064
		void* N00000515; //0x0068 
		char pad_0x006C[0xC]; //0x006C
		void* N00000519; //0x0078 
		char pad_0x007C[0x44]; //0x007C

	}; //Size=0x00C0

	struct settings_cfg_s
	{
		uint32_t resolution_index;
		uint32_t unk1;
		uint32_t aspect_ratio_index;
		uint32_t texture_quality;
		uint32_t texture_filter;
		uint32_t view_distance; // 0-100
		uint32_t detail_distance; // 0-100
		uint32_t vehicle_density; // 0-100
		uint32_t sharpness;
		uint32_t vsync;
		uint32_t nightshadow_quality;
		uint32_t shadow_quality;
		uint32_t reflection_quality;
		uint32_t water_quality;
	};

	struct resolution_mode_s
	{
		uint32_t width;
		uint32_t height;
		uint32_t hz;
		uint32_t format;
	};

	struct resolution_modes_ptr
	{
		resolution_mode_s* modes;
	};
}
