// RenderEffect.cpp
// KlayGE 渲染效果类 实现文件
// Ver 3.6.0
// 版权所有(C) 龚敏敏, 2003-2007
// Homepage: http://klayge.sourceforge.net
//
// 3.6.0
// 增加了Clone (2007.6.11)
//
// 3.5.0
// 改用基于xml的特效格式 (2006.10.21)
//
// 3.2.0
// 支持了bool类型 (2006.3.8)
//
// 3.0.0
// 增加了RenderTechnique和RenderPass (2005.9.4)
//
// 2.8.0
// 增加了Do*函数，使用模板方法模式 (2005.7.24)
// 使用新的自动更新参数的方法 (2005.7.25)
//
// 2.2.0
// 统一使用istream作为资源标示符 (2004.10.26)
//
// 2.1.2
// 增加了Parameter (2004.5.26)
//
// 2.0.3
// 初次建立 (2003.3.2)
// 修改了SetTexture的参数 (2004.3.6)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Sampler.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/RenderStateObject.hpp>
#include <KlayGE/ShaderObject.hpp>

#include <sstream>
#include <boost/typeof/typeof.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <KlayGE/RenderEffect.hpp>

namespace
{
	using namespace KlayGE;

	class type_define
	{
	public:
		static type_define& instance()
		{
			static type_define ret;
			return ret;
		}

		uint32_t type_code(std::string const & name) const
		{
			for (uint32_t i = 0; i < types_.size(); ++ i)
			{
				if (types_[i] == name)
				{
					return i;
				}
			}
			BOOST_ASSERT(false);
			return 0xFFFFFFFF;
		}

		std::string const & type_name(uint32_t code) const
		{
			if (code < types_.size())
			{
				return types_[code];
			}
			BOOST_ASSERT(false);

			static std::string empty_str("");
			return empty_str;
		}

	private:
		type_define()
		{
			types_.push_back("bool");
			types_.push_back("dword");
			types_.push_back("string");
			types_.push_back("sampler1D");
			types_.push_back("sampler2D");
			types_.push_back("sampler3D");
			types_.push_back("samplerCUBE");
			types_.push_back("shader");
			types_.push_back("int");
			types_.push_back("int2");
			types_.push_back("int3");
			types_.push_back("int4");
			types_.push_back("float");
			types_.push_back("float2");
			types_.push_back("float2x2");
			types_.push_back("float2x3");
			types_.push_back("float2x4");
			types_.push_back("float3");
			types_.push_back("float3x2");
			types_.push_back("float3x3");
			types_.push_back("float3x4");
			types_.push_back("float4");
			types_.push_back("float4x2");
			types_.push_back("float4x3");
			types_.push_back("float4x4");
		}

	private:
		std::vector<std::string> types_;
	};

	std::string read_short_string(ResIdentifierPtr const & source)
	{
		uint8_t len;
		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		std::string ret(len, '\0');
		source->read(&ret[0], len);

		return ret;
	}

	RenderVariablePtr read_var(ResIdentifierPtr const & source, uint32_t type, uint32_t array_size)
	{
		RenderVariablePtr var;

		switch (type)
		{
		case REDT_bool:
			if (0 == array_size)
			{
				var.reset(new RenderVariableBool);

				bool tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			break;

		case REDT_dword:
		case REDT_int:
			if (0 == array_size)
			{
				var.reset(new RenderVariableInt);

				int tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			break;

		case REDT_string:
			{
				var.reset(new RenderVariableString);
				*var = read_short_string(source);
			}
			break;

		case REDT_sampler1D:
		case REDT_sampler2D:
		case REDT_sampler3D:
		case REDT_samplerCUBE:
			{
				var.reset(new RenderVariableSampler);
				SamplerPtr s(new Sampler);

				uint32_t tmp_int;
				source->read(reinterpret_cast<char*>(&tmp_int), sizeof(tmp_int));
				s->filter = static_cast<Sampler::TexFilterOp>(tmp_int);

				source->read(reinterpret_cast<char*>(&tmp_int), sizeof(tmp_int));
				s->addr_mode_u = static_cast<Sampler::TexAddressingMode>(tmp_int);
				source->read(reinterpret_cast<char*>(&tmp_int), sizeof(tmp_int));
				s->addr_mode_v = static_cast<Sampler::TexAddressingMode>(tmp_int);
				source->read(reinterpret_cast<char*>(&tmp_int), sizeof(tmp_int));
				s->addr_mode_w = static_cast<Sampler::TexAddressingMode>(tmp_int);

				source->read(reinterpret_cast<char*>(&tmp_int), sizeof(tmp_int));
				s->anisotropy = static_cast<uint8_t>(tmp_int);

				source->read(reinterpret_cast<char*>(&tmp_int), sizeof(tmp_int));
				s->max_mip_level = static_cast<uint8_t>(tmp_int);

				float tmp_float;
				source->read(reinterpret_cast<char*>(&tmp_float), sizeof(tmp_float));
				s->mip_map_lod_bias = tmp_float;

				Color border_clr;
				source->read(reinterpret_cast<char*>(&border_clr), sizeof(border_clr));
				s->border_clr = border_clr;

				*var = s;
			}
			break;

		case REDT_shader:
			{
				var.reset(new RenderVariableShader);

				shader_desc desc;

				desc.profile = read_short_string(source);

				std::string value = read_short_string(source);
				desc.func_name = value.substr(0, value.find("("));

				*var = desc;
			}
			break;

		case REDT_float:
			if (0 == array_size)
			{
				var.reset(new RenderVariableFloat);

				float tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			else
			{
				var.reset(new RenderVariableFloatArray);
			}
			break;

		case REDT_float2:
			{
				var.reset(new RenderVariableFloat2);

				float2 tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			break;

		case REDT_float3:
			{
				var.reset(new RenderVariableFloat3);

				float3 tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			break;

		case REDT_float4:
			if (0 == array_size)
			{
				var.reset(new RenderVariableFloat4);

				float4 tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			else
			{
				var.reset(new RenderVariableFloat4Array);
			}
			break;

		case REDT_float4x4:
			if (0 == array_size)
			{
				var.reset(new RenderVariableFloat4x4);

				float4x4 tmp;
				source->read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
				*var = tmp;
			}
			else
			{
				var.reset(new RenderVariableFloat4x4Array);
			}
			break;

		default:
			BOOST_ASSERT(false);
			break;
		}

		return var;
	}

	int get_index_from_state_name(std::string const & state_name)
	{
		int index;
		std::string::size_type s = state_name.find("[");
		if (std::string::npos == s)
		{
			index = 0;
		}
		else
		{
			std::string::size_type e = state_name.find("]");
			index = boost::lexical_cast<int>(state_name.substr(s + 1, e - s));
		}

		return index;
	}
}

namespace KlayGE
{
	void RenderEffectAnnotation::Load(ResIdentifierPtr const & source)
	{
		source->read(reinterpret_cast<char*>(&type_), sizeof(type_));
		name_ = read_short_string(source);
		var_ = read_var(source, type_, 0);
	}


	class NullRenderEffect : public RenderEffect
	{
	public:
		NullRenderEffect()
		{
		}

		RenderTechniquePtr MakeRenderTechnique()
		{
			return RenderTechnique::NullObject();
		}
	};

	RenderEffect::RenderEffect()
	{
	}

	void RenderEffect::Load(ResIdentifierPtr const & source)
	{
	#pragma pack(push, 1)
		struct fxml_header
		{
			uint32_t fourcc;
			uint32_t ver;
			uint32_t num_parameters;
			uint32_t num_shaders;
			uint32_t num_techniques;
		};
	#pragma pack(pop)

		if (source)
		{
			fxml_header header;
			source->read(reinterpret_cast<char*>(&header), sizeof(header));
			BOOST_ASSERT((MakeFourCC<'F', 'X', 'M', 'L'>::value == header.fourcc));
			BOOST_ASSERT(1 == header.ver);

			for (uint32_t i = 0; i < header.num_parameters; ++ i)
			{
				params_.push_back(RenderEffectParameterPtr(new RenderEffectParameter(*this)));
				params_[i]->Load(source);
			}

			if (header.num_shaders > 0)
			{
				shaders_.reset(new BOOST_TYPEOF(*shaders_));
				for (uint32_t i = 0; i < header.num_shaders; ++ i)
				{
					shaders_->push_back(RenderShaderFunc());
					(*shaders_)[i].Load(source);
				}
			}

			for (uint32_t i = 0; i < header.num_techniques; ++ i)
			{
				techniques_.push_back(RenderTechniquePtr(new RenderTechnique(*this)));
				techniques_[i]->Load(source);
			}
		}
	}

	RenderEffectPtr RenderEffect::Clone()
	{
		RenderEffectPtr ret(new RenderEffect);

		ret->prototype_effect_ = prototype_effect_;
		ret->shaders_ = shaders_;

		ret->params_.resize(params_.size());
		for (size_t i = 0; i < params_.size(); ++ i)
		{
			ret->params_[i] = params_[i]->Clone(*ret);
		}

		ret->techniques_.resize(techniques_.size());
		for (size_t i = 0; i < techniques_.size(); ++ i)
		{
			ret->techniques_[i] = techniques_[i]->Clone(*ret);
		}

		return ret;
	}

	RenderEffectPtr RenderEffect::NullObject()
	{
		static RenderEffectPtr obj(new NullRenderEffect);
		return obj;
	}

	RenderEffectParameterPtr RenderEffect::ParameterByName(std::string const & name) const
	{
		BOOST_FOREACH(BOOST_TYPEOF(params_)::const_reference param, params_)
		{
			if (name == *param->Name())
			{
				return param;
			}
		}
		return RenderEffectParameter::NullObject();
	}

	RenderEffectParameterPtr RenderEffect::ParameterBySemantic(std::string const & semantic) const
	{
		BOOST_FOREACH(BOOST_TYPEOF(params_)::const_reference param, params_)
		{
			if (semantic == *param->Semantic())
			{
				return param;
			}
		}
		return RenderEffectParameter::NullObject();
	}

	RenderTechniquePtr RenderEffect::TechniqueByName(std::string const & name) const
	{
		BOOST_FOREACH(BOOST_TYPEOF(techniques_)::const_reference tech, techniques_)
		{
			if (name == tech->Name())
			{
				return tech;
			}
		}
		return RenderTechnique::NullObject();
	}


	class NullRenderTechnique : public RenderTechnique
	{
	public:
		NullRenderTechnique()
			: RenderTechnique(*RenderEffect::NullObject())
		{
			is_validate_ = true;
		}

	private:
		void DoBegin()
		{
		}
		void DoEnd()
		{
		}

		RenderPassPtr MakeRenderPass()
		{
			return RenderPass::NullObject();
		}
	};

	RenderTechniquePtr RenderTechnique::NullObject()
	{
		static RenderTechniquePtr obj(new NullRenderTechnique);
		return obj;
	}

	void RenderTechnique::Load(ResIdentifierPtr const & source)
	{
		name_.reset(new BOOST_TYPEOF(*name_)(read_short_string(source)));
		source->read(reinterpret_cast<char*>(&weight_), sizeof(weight_));

		uint32_t len;
		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		if (len > 0)
		{
			annotations_.reset(new BOOST_TYPEOF(*annotations_));
			for (size_t i = 0; i < len; ++ i)
			{
				annotations_->push_back(RenderEffectAnnotationPtr(new RenderEffectAnnotation));
				(*annotations_)[i]->Load(source);
			}
		}

		is_validate_ = true;

		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		for (uint32_t i = 0; i < len; ++ i)
		{
			passes_.push_back(RenderPassPtr(new RenderPass(effect_)));
			passes_[i]->Load(source);

			is_validate_ &= passes_[i]->Validate();
		}
	}

	RenderTechniquePtr RenderTechnique::Clone(RenderEffect& effect)
	{
		RenderTechniquePtr ret(new RenderTechnique(effect));

		ret->name_ = name_;

		ret->annotations_ = annotations_;
		ret->weight_ = weight_;
		ret->is_validate_ = is_validate_;

		ret->passes_.resize(passes_.size());
		for (size_t i = 0; i < passes_.size(); ++ i)
		{
			ret->passes_[i] = passes_[i]->Clone(effect);
		}

		return ret;
	}


	class NullRenderPass : public RenderPass
	{
	public:
		NullRenderPass()
			: RenderPass(*RenderEffect::NullObject())
		{
			is_validate_ = true;
		}
	};

	RenderPassPtr RenderPass::NullObject()
	{
		static RenderPassPtr obj(new NullRenderPass);
		return obj;
	}

	void RenderPass::Load(ResIdentifierPtr const & source)
	{
		RenderFactory& rf = Context::Instance().RenderFactoryInstance();

		name_.reset(new BOOST_TYPEOF(*name_)(read_short_string(source)));

		uint32_t len;
		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		if (len > 0)
		{
			annotations_.reset(new BOOST_TYPEOF(*annotations_));
			for (size_t i = 0; i < len; ++ i)
			{
				annotations_->push_back(RenderEffectAnnotationPtr(new RenderEffectAnnotation));
				(*annotations_)[i]->Load(source);
			}
		}

		RasterizerStateDesc rs_desc;
		DepthStencilStateDesc dss_desc;
		BlendStateDesc bs_desc;
		shader_obj_ = rf.MakeShaderObject();

		shader_descs_.reset(new BOOST_TYPEOF(*shader_descs_));
		shader_descs_->resize(ShaderObject::ST_NumShaderTypes);

		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		for (size_t i = 0; i < len; ++ i)
		{
			uint32_t type;
			source->read(reinterpret_cast<char*>(&type), sizeof(type));
			std::string state_name = read_short_string(source);
			RenderVariablePtr var = read_var(source, type, 0);

			if (REDT_shader != type)
			{
				uint32_t state_val;
				switch (type)
				{
				case REDT_bool:
					{
						bool tmp;
						var->Value(tmp);
						state_val = tmp;
					}
					break;

				case REDT_int:
					{
						int tmp;
						var->Value(tmp);
						state_val = tmp;
					}
					break;

				case REDT_float:
					{
						float tmp;
						var->Value(tmp);
						state_val = float_to_uint32(tmp);
					}
					break;

				default:
					BOOST_ASSERT(false);
					state_val = 0;
					break;
				}

				if ("polygon_mode" == state_name)
				{
					rs_desc.polygon_mode = static_cast<PolygonMode>(state_val);
				}
				if ("shade_mode" == state_name)
				{
					rs_desc.shade_mode = static_cast<ShadeMode>(state_val);
				}
				if ("cull_mode" == state_name)
				{
					rs_desc.cull_mode = static_cast<CullMode>(state_val);
				}
				if ("front_face_ccw" == state_name)
				{
					rs_desc.front_face_ccw = state_val ? true : false;
				}
				if ("polygon_offset_factor" == state_name)
				{
					rs_desc.polygon_offset_factor = uint32_to_float(state_val);
				}
				if ("polygon_offset_units" == state_name)
				{
					rs_desc.polygon_offset_units = uint32_to_float(state_val);
				}
				if ("scissor_enable" == state_name)
				{
					rs_desc.scissor_enable = state_val ? true : false;
				}
				if ("multisample_enable" == state_name)
				{
					rs_desc.multisample_enable = state_val ? true : false;
				}

				if ("alpha_to_coverage_enable" == state_name)
				{
					bs_desc.alpha_to_coverage_enable = state_val ? true : false;
				}
				if ("independent_blend_enable" == state_name)
				{
					bs_desc.independent_blend_enable = state_val ? true : false;
				}
				if (0 == state_name.find("blend_enable"))
				{
					bs_desc.blend_enable[get_index_from_state_name(state_name)] = state_val ? true : false;					
				}
				if (0 == state_name.find("blend_op"))
				{
					bs_desc.blend_op[get_index_from_state_name(state_name)] = static_cast<BlendOperation>(state_val);
				}
				if (0 == state_name.find("src_blend"))
				{
					bs_desc.src_blend[get_index_from_state_name(state_name)] = static_cast<AlphaBlendFactor>(state_val);
				}
				if (0 == state_name.find("dest_blend"))
				{
					bs_desc.dest_blend[get_index_from_state_name(state_name)] = static_cast<AlphaBlendFactor>(state_val);
				}
				if (0 == state_name.find("blend_op_alpha"))
				{
					bs_desc.blend_op_alpha[get_index_from_state_name(state_name)] = static_cast<BlendOperation>(state_val);
				}
				if (0 == state_name.find("src_blend_alpha"))
				{
					bs_desc.src_blend_alpha[get_index_from_state_name(state_name)] = static_cast<AlphaBlendFactor>(state_val);
				}
				if (0 == state_name.find("dest_blend_alpha"))
				{
					bs_desc.dest_blend_alpha[get_index_from_state_name(state_name)] = static_cast<AlphaBlendFactor>(state_val);
				}
				if (0 == state_name.find("color_write_mask"))
				{
					bs_desc.color_write_mask[get_index_from_state_name(state_name)] = static_cast<uint8_t>(state_val);
				}

				if ("depth_enable" == state_name)
				{
					dss_desc.depth_enable = state_val ? true : false;
				}
				if ("depth_write_mask" == state_name)
				{
					dss_desc.depth_write_mask = state_val ? true : false;
				}
				if ("depth_func" == state_name)
				{
					dss_desc.depth_func = static_cast<CompareFunction>(state_val);
				}

				if ("front_stencil_enable" == state_name)
				{
					dss_desc.front_stencil_enable = state_val ? true : false;
				}
				if ("front_stencil_func" == state_name)
				{
					dss_desc.front_stencil_func = static_cast<CompareFunction>(state_val);
				}
				if ("front_stencil_ref" == state_name)
				{
					front_stencil_ref_ = static_cast<uint16_t>(state_val);
				}
				if ("front_stencil_read_mask" == state_name)
				{
					dss_desc.front_stencil_read_mask = static_cast<uint16_t>(state_val);
				}
				if ("front_stencil_write_mask" == state_name)
				{
					dss_desc.front_stencil_write_mask = static_cast<uint16_t>(state_val);
				}
				if ("front_stencil_fail" == state_name)
				{
					dss_desc.front_stencil_fail = static_cast<StencilOperation>(state_val);
				}
				if ("front_stencil_depth_fail" == state_name)
				{
					dss_desc.front_stencil_depth_fail = static_cast<StencilOperation>(state_val);
				}
				if ("front_stencil_pass" == state_name)
				{
					dss_desc.front_stencil_pass = static_cast<StencilOperation>(state_val);
				}
				if ("back_stencil_enable" == state_name)
				{
					dss_desc.back_stencil_enable = state_val ? true : false;
				}
				if ("back_stencil_func" == state_name)
				{
					dss_desc.back_stencil_func = static_cast<CompareFunction>(state_val);
				}
				if ("back_stencil_ref" == state_name)
				{
					back_stencil_ref_ = static_cast<uint16_t>(state_val);
				}
				if ("back_stencil_read_mask" == state_name)
				{
					dss_desc.back_stencil_read_mask = static_cast<uint16_t>(state_val);
				}
				if ("back_stencil_write_mask" == state_name)
				{
					dss_desc.back_stencil_write_mask = static_cast<uint16_t>(state_val);
				}
				if ("back_stencil_fail" == state_name)
				{
					dss_desc.back_stencil_fail = static_cast<StencilOperation>(state_val);
				}
				if ("back_stencil_depth_fail" == state_name)
				{
					dss_desc.back_stencil_depth_fail = static_cast<StencilOperation>(state_val);
				}
				if ("back_stencil_pass" == state_name)
				{
					dss_desc.back_stencil_pass = static_cast<StencilOperation>(state_val);
				}
			}
			else
			{
				if ("vertex_shader" == state_name)
				{
					shader_desc& sd = (*shader_descs_)[ShaderObject::ST_VertexShader];
					var->Value(sd);
				}
				if ("pixel_shader" == state_name)
				{
					shader_desc& sd = (*shader_descs_)[ShaderObject::ST_PixelShader];
					var->Value(sd);
				}
			}
		}

		rasterizer_state_obj_ = rf.MakeRasterizerStateObject(rs_desc);
		depth_stencil_state_obj_ = rf.MakeDepthStencilStateObject(dss_desc);
		blend_state_obj_ = rf.MakeBlendStateObject(bs_desc);

		shader_text_.reset(new BOOST_TYPEOF(*shader_text_)(this->GenShaderText()));
		for (size_t i = 0; i < ShaderObject::ST_NumShaderTypes; ++ i)
		{
			shader_obj_->SetShader(effect_, static_cast<ShaderObject::ShaderType>(i), shader_descs_, shader_text_);
		}

		is_validate_ = shader_obj_->Validate();
	}

	RenderPassPtr RenderPass::Clone(RenderEffect& effect)
	{
		RenderPassPtr ret(new RenderPass(effect));

		ret->name_ = name_;
		ret->annotations_ = annotations_;
		ret->shader_descs_ = shader_descs_;
		ret->shader_text_ = shader_text_;

		ret->rasterizer_state_obj_ = rasterizer_state_obj_;
		ret->depth_stencil_state_obj_ = depth_stencil_state_obj_;
		ret->blend_state_obj_ = blend_state_obj_;
		ret->shader_obj_ = shader_obj_->Clone(effect);

		ret->is_validate_ = is_validate_;

		return ret;
	}

	void RenderPass::Apply()
	{
		RenderEngine& render_eng = Context::Instance().RenderFactoryInstance().RenderEngineInstance();
		render_eng.SetStateObjects(rasterizer_state_obj_, depth_stencil_state_obj_, front_stencil_ref_, back_stencil_ref_, blend_state_obj_, shader_obj_);
	}

	std::string RenderPass::GenShaderText() const
	{
		std::stringstream ss;
		for (uint32_t i = 0; i < effect_.NumParameters(); ++ i)
		{
			RenderEffectParameter& param = *effect_.ParameterByIndex(i);

			ss << type_define::instance().type_name(param.type()) << " " << *param.Name();
			if (param.ArraySize() != 0)
			{
				ss << "[" << param.ArraySize() << "]";
			}

			ss << ";" << std::endl;
		}

		for (uint32_t i = 0; i < effect_.NumShaders(); ++ i)
		{
			ss << effect_.ShaderByIndex(i).str() << std::endl;
		}

		return ss.str();
	}



	class NullRenderEffectParameter : public RenderEffectParameter
	{
	public:
		NullRenderEffectParameter()
			: RenderEffectParameter(*RenderEffect::NullObject())
		{
		}

		RenderEffectParameter& operator=(bool const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(int const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(float const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(float2 const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(float3 const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(float4 const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(float4x4 const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(SamplerPtr const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(std::vector<bool> const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(std::vector<int> const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(std::vector<float> const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(std::vector<float4> const & /*value*/)
		{
			return *this;
		}
		RenderEffectParameter& operator=(std::vector<float4x4> const & /*value*/)
		{
			return *this;
		}

		void Value(bool& val) const
		{
			val = false;
		}
		void Value(int& val) const
		{
			val = 0;
		}
		void Value(float& val) const
		{
			val = 0;
		}
		void Value(float2& val) const
		{
			val = float2::Zero();
		}
		void Value(float3& val) const
		{
			val = float3::Zero();
		}
		void Value(float4& val) const
		{
			val = float4::Zero();
		}
		void Value(float4x4& val) const
		{
			val = float4x4::Identity();
		}
		void Value(SamplerPtr& val) const
		{
			val = SamplerPtr();
		}
		void Value(std::vector<bool>& val) const
		{
			val.clear();
		}
		void Value(std::vector<int>& val) const
		{
			val.clear();
		}
		void Value(std::vector<float>& val) const
		{
			val.clear();
		}
		void Value(std::vector<float4>& val) const
		{
			val.clear();
		}
		void Value(std::vector<float4x4>& val) const
		{
			val.clear();
		}

		void Flush()
		{
		}

	private:
		NullRenderEffectParameter(NullRenderEffectParameter const & rhs);
		NullRenderEffectParameter& operator=(NullRenderEffectParameter const & rhs);
	};


	RenderEffectParameter::RenderEffectParameter(RenderEffect& effect)
		: effect_(effect)
	{
	}

	RenderEffectParameter::~RenderEffectParameter()
	{
	}

	void RenderEffectParameter::Load(ResIdentifierPtr const & source)
	{
		source->read(reinterpret_cast<char*>(&array_size_), sizeof(array_size_));
		source->read(reinterpret_cast<char*>(&type_), sizeof(type_));
		name_.reset(new BOOST_TYPEOF(*name_)(read_short_string(source)));
		var_ = read_var(source, type_, array_size_);

		uint32_t len;
		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		if (len > 0)
		{
			annotations_.reset(new BOOST_TYPEOF(*annotations_));
			for (size_t i = 0; i < len; ++ i)
			{
				annotations_->push_back(RenderEffectAnnotationPtr(new RenderEffectAnnotation));
				(*annotations_)[i]->Load(source);
			}
		}

		semantic_.reset(new BOOST_TYPEOF(*semantic_)(read_short_string(source)));
	}

	RenderEffectParameterPtr RenderEffectParameter::Clone(RenderEffect& effect)
	{
		RenderEffectParameterPtr ret(new RenderEffectParameter(effect));

		ret->name_ = name_;
		ret->semantic_ = semantic_;

		ret->type_ = type_;
		ret->var_ = var_->Clone();
		ret->array_size_ = array_size_;

		ret->annotations_ = annotations_;

		return ret;
	}

	RenderEffectParameterPtr RenderEffectParameter::NullObject()
	{
		static RenderEffectParameterPtr obj(new NullRenderEffectParameter);
		return obj;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(bool const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(int const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(float const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(float2 const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(float3 const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(float4 const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(float4x4 const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(TexturePtr const & value)
	{
		SamplerPtr s;
		var_->Value(s);
		BOOST_ASSERT(s);

		s->texture = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(std::vector<bool> const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(std::vector<int> const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(std::vector<float> const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(std::vector<float4> const & value)
	{
		*var_ = value;
		return *this;
	}

	RenderEffectParameter& RenderEffectParameter::operator=(std::vector<float4x4> const & value)
	{
		*var_ = value;
		return *this;
	}

	void RenderEffectParameter::Value(bool& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(int& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(float& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(float2& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(float3& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(float4& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(float4x4& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(SamplerPtr& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(std::vector<bool>& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(std::vector<int>& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(std::vector<float>& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(std::vector<float4>& val) const
	{
		var_->Value(val);
	}

	void RenderEffectParameter::Value(std::vector<float4x4>& val) const
	{
		var_->Value(val);
	}


	void RenderShaderFunc::Load(ResIdentifierPtr const & source)
	{
		uint32_t len;
		source->read(reinterpret_cast<char*>(&len), sizeof(len));
		str_.resize(len);
		source->read(&str_[0], len);
	}


	RenderVariable::RenderVariable()
		: dirty_(true)
	{
	}

	RenderVariable::~RenderVariable()
	{
	}

	RenderVariable& RenderVariable::operator=(bool const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(int const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(float const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(float2 const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(float3 const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(float4 const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(float4x4 const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(SamplerPtr const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(std::string const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(shader_desc const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(std::vector<bool> const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(std::vector<int> const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(std::vector<float> const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(std::vector<float4> const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	RenderVariable& RenderVariable::operator=(std::vector<float4x4> const & /*value*/)
	{
		BOOST_ASSERT(false);
		return *this;
	}

	void RenderVariable::Value(bool& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(int& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(float& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(float2& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(float3& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(float4& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(float4x4& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(SamplerPtr& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(std::string& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(shader_desc& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(std::vector<bool>& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(std::vector<int>& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(std::vector<float>& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(std::vector<float4>& /*value*/) const
	{
		BOOST_ASSERT(false);
	}

	void RenderVariable::Value(std::vector<float4x4>& /*value*/) const
	{
		BOOST_ASSERT(false);
	}
}
