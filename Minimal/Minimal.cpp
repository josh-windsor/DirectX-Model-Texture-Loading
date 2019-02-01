#include "Framework.h"

#include "ShaderSet.h"
#include "Mesh.h"
#include "Texture.h"

//================================================================================
// Minimal Application
// An example of how to use selected parts of this framework.
//================================================================================
class MinimalApp : public FrameworkApp
{
public:

	struct PerFrameCBData
	{
		m4x4 m_matProjection;
		m4x4 m_matView;
		f32		m_time;
		f32     m_padding[3];
	};

	struct PerDrawCBData
	{
		m4x4 m_matMVP;
	};

	void on_init(SystemsInterface& systems) override
	{
		m_position = v3(0.5f, 0.5f, 0.5f);
		m_size = 1.0f;
		systems.pCamera->eye = v3(10.f, 5.f, 7.f);
		systems.pCamera->look_at(v3(3.f, 0.5f, 0.f));

		// compile a set of shaders
		m_meshShader.init(systems.pD3DDevice
			, ShaderSetDesc::Create_VS_PS("Assets/Shaders/MinimalShaders.fx", "VS_Mesh", "PS_Mesh")
			, { VertexFormatTraits<MeshVertex>::desc, VertexFormatTraits<MeshVertex>::size }
		);

		// Create Per Frame Constant Buffer.
		m_pPerFrameCB = create_constant_buffer<PerFrameCBData>(systems.pD3DDevice);

		// Create Per Frame Constant Buffer.
		m_pPerDrawCB = create_constant_buffer<PerDrawCBData>(systems.pD3DDevice);

		// Initialize a mesh directly.
		create_mesh_cube(systems.pD3DDevice, m_meshArray[0], 0.5f);

		// Initialize a mesh from an .OBJ file
		create_mesh_from_obj(systems.pD3DDevice, m_meshArray[1], "Assets/Models/apple.obj", 0.01f);

		// Initialise some textures;
		m_textures[0].init_from_dds(systems.pD3DDevice, "Assets/Textures/brick.dds");
		m_textures[1].init_from_dds(systems.pD3DDevice, "Assets/Textures/apple_diffuse.dds");

		// We need a sampler state to define wrapping and mipmap parameters.
		m_pLinearMipSamplerState = create_basic_sampler(systems.pD3DDevice, D3D11_TEXTURE_ADDRESS_WRAP);

		// Setup per-frame data
		m_perFrameCBData.m_time = 0.0f;
	}

	void on_update(SystemsInterface& systems) override
	{
		//////////////////////////////////////////////////////////////////////////
		// You can use features from the ImGui library.
		// Investigate the ImGui::ShowDemoWindow() function for ideas.
		// see also : https://github.com/ocornut/imgui
		//////////////////////////////////////////////////////////////////////////

		// This function displays some useful debugging values, camera positions etc.
		DemoFeatures::editorHud(systems.pDebugDrawContext);

		ImGui::SliderFloat3("Position", (float*)&m_position, -1.f, 1.f);
		ImGui::SliderFloat("Size", &m_size, 0.1f, 10.f);

		// Update Per Frame Data.
		m_perFrameCBData.m_matProjection = systems.pCamera->projMatrix.Transpose();
		m_perFrameCBData.m_matView = systems.pCamera->viewMatrix.Transpose();
		m_perFrameCBData.m_time += 0.001f;
	}

	void on_render(SystemsInterface& systems) override
	{
		//////////////////////////////////////////////////////////////////////////
		// Imgui can also be used inside the render function.
		//////////////////////////////////////////////////////////////////////////


		//////////////////////////////////////////////////////////////////////////
		// You can use features from the DebugDrawlibrary.
		// Investigate the following functions for ideas.
		// see also : https://github.com/glampert/debug-draw
		//////////////////////////////////////////////////////////////////////////

		// Grid from -50 to +50 in both X & Z
		auto ctx = systems.pDebugDrawContext;

		dd::xzSquareGrid(ctx, -50.0f, 50.0f, 0.0f, 1.f, dd::colors::DimGray);
		dd::axisTriad(ctx, (const float*)& m4x4::Identity, 0.1f, 15.0f);
		dd::box(ctx, (const float*)&m_position, dd::colors::Blue, m_size, m_size, m_size);
		if (systems.pCamera->pointInFrustum(m_position))
		{
			dd::projectedText(ctx, "A Box", (const float*)&m_position, dd::colors::White, (const float*)&systems.pCamera->vpMatrix, 0, 0, systems.width, systems.height, 0.5f);
		}

		// Push Per Frame Data to GPU
		D3D11_MAPPED_SUBRESOURCE subresource;
		if (!FAILED(systems.pD3DContext->Map(m_pPerFrameCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource)))
		{
			memcpy(subresource.pData, &m_perFrameCBData, sizeof(PerFrameCBData));
			systems.pD3DContext->Unmap(m_pPerFrameCB, 0);
		}

		// Bind our set of shaders.
		m_meshShader.bind(systems.pD3DContext);

		// Bind Constant Buffers, to both PS and VS stages
		ID3D11Buffer* buffers[] = { m_pPerFrameCB, m_pPerDrawCB };
		systems.pD3DContext->VSSetConstantBuffers(0, 2, buffers);
		systems.pD3DContext->PSSetConstantBuffers(0, 2, buffers);

		// Bind a sampler state
		ID3D11SamplerState* samplers[] = { m_pLinearMipSamplerState };
		systems.pD3DContext->PSSetSamplers(0, 1, samplers);


		constexpr f32 kGridSpacing = 1.5f;
		constexpr u32 kNumInstances = 5;
		constexpr u32 kNumModelTypes = 2;

		for(u32 i = 0; i < kNumModelTypes; ++i)
		{
			// Bind a mesh and texture.
			m_meshArray[i].bind(systems.pD3DContext);
			m_textures[i].bind(systems.pD3DContext, ShaderStage::kPixel, 0);

			// Draw several instances
			for (u32 j = 0; j < kNumInstances; ++j)
			{
				// Compute MVP matrix.
				m4x4 matModel = m4x4::CreateTranslation(v3(j * kGridSpacing, i * kGridSpacing, 0.f));
				m4x4 matMVP = matModel * systems.pCamera->vpMatrix;

				// Update Per Draw Data
				m_perDrawCBData.m_matMVP = matMVP.Transpose();

				// Push to GPU
				push_constant_buffer(systems.pD3DContext, m_pPerDrawCB, m_perDrawCBData);

				// Draw the mesh.
				m_meshArray[i].draw(systems.pD3DContext);
			}
		}

	}

	void on_resize(SystemsInterface& ) override
	{
		
	}

private:

	PerFrameCBData m_perFrameCBData;
	ID3D11Buffer* m_pPerFrameCB = nullptr;

	PerDrawCBData m_perDrawCBData;
	ID3D11Buffer* m_pPerDrawCB = nullptr;

	ShaderSet m_meshShader;
	
	Mesh m_meshArray[2];
	Texture m_textures[2];
	ID3D11SamplerState* m_pLinearMipSamplerState = nullptr;

	v3 m_position;
	f32 m_size;
};

MinimalApp g_app;

FRAMEWORK_IMPLEMENT_MAIN(g_app, "Minimal")
