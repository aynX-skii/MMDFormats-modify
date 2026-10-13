#include <utility>
#include <stdexcept>
#include <limits>
#include "Pmx.h"
#ifndef __unix__
#include "EncodingHelper.h"
#else
#include <unicode/ucnv.h>
#endif

namespace pmx
{
	static void ReadChecked(std::istream* stream, char* data, std::streamsize size)
	{
		if (!stream || size < 0 || !stream->read(data, size))
			throw std::runtime_error("Truncated PMX data");
	}

	static void ReadCount(std::istream* stream, int& count, size_t minimumBytes = 1)
	{
		ReadChecked(stream, reinterpret_cast<char*>(&count), sizeof(count));
		const auto position = stream->tellg();
		std::streamoff remaining = -1;
		if (position != std::istream::pos_type(-1)) {
			stream->seekg(0, std::ios::end);
			const auto end = stream->tellg();
			stream->clear();
			stream->seekg(position);
			if (end != std::istream::pos_type(-1)) remaining = end - position;
		} else {
			stream->clear();
			remaining = stream->rdbuf()->in_avail();
		}
		if (count < 0 || count > 16000000 ||
			(remaining >= 0 && static_cast<uint64_t>(count) * minimumBytes > static_cast<uint64_t>(remaining)))
			throw std::runtime_error("Invalid PMX element count");
	}
	/// インデックス値を読み込む
	int ReadIndex(std::istream *stream, int size)
	{
		switch (size)
		{
		case 1:
			uint8_t tmp8;
			ReadChecked(stream, (char*) &tmp8, sizeof(uint8_t));
			if (255 == tmp8)
			{
				return -1;
			}
			else {
				return (int) tmp8;
			}
		case 2:
			uint16_t tmp16;
			ReadChecked(stream, (char*) &tmp16, sizeof(uint16_t));
			if (65535 == tmp16)
			{
				return -1;
			}
			else {
				return (int) tmp16;
			}
		case 4:
			int tmp32;
			ReadChecked(stream, (char*) &tmp32, sizeof(int));
			return tmp32;
		default:
			return -1;
		}
	}

	/// 文字列を読み込む
	utfstring ReadString(std::istream *stream, uint8_t encoding)
	{
#ifndef __unix__
		oguna::EncodingConverter converter = oguna::EncodingConverter();
#endif
		int size;
		ReadCount(stream, size);
		std::vector<char> buffer;
		if (size == 0)
		{
#ifndef __unix__
			return utfstring(L"");
#else
			return utfstring("");
#endif
		}
		buffer.resize(size);
		ReadChecked(stream, (char*) buffer.data(), size);
		if (encoding == 0)
		{
			if (size % 2) throw std::runtime_error("Odd UTF-16 PMX string length");
			// UTF16
#ifndef __unix__
			return utfstring((wchar_t*) buffer.data(), size / 2);
#else
			utfstring result;
			std::vector<char> outbuf;
			outbuf.resize(size*2);

			// Always remember to set U_ZERO_ERROR before calling ucnv_convert(),
			// otherwise the function will fail.
			UErrorCode err = U_ZERO_ERROR;
			size = ucnv_convert("UTF-8", "UTF-16LE", (char*)outbuf.data(), outbuf.capacity(), buffer.data(), size, &err);
			if(!U_SUCCESS(err)) {
				std::cout << "oops, something wrong?" << std::endl;
				std::cout << u_errorName(err) << std::endl;
				throw std::runtime_error("Invalid PMX string encoding");
			}

			result.assign((const char*)outbuf.data(), size);
			return result;
#endif
		}
		else
		{
			// UTF8
#ifndef __unix__
			utfstring result;
			converter.Utf8ToUtf16(buffer.data(), size, &result);
			return result;
#else
			return utfstring((const char*)buffer.data(), size);
#endif
		}
	}

	void PmxSetting::Read(std::istream *stream)
	{
		uint8_t count;
		ReadChecked(stream, (char*) &count, sizeof(uint8_t));
		if (count < 8)
		{
			throw std::runtime_error("Invalid or unsupported PMX data");
		}
		ReadChecked(stream, (char*) &encoding, sizeof(uint8_t));
		ReadChecked(stream, (char*) &uv, sizeof(uint8_t));
		ReadChecked(stream, (char*) &vertex_index_size, sizeof(uint8_t));
		ReadChecked(stream, (char*) &texture_index_size, sizeof(uint8_t));
		ReadChecked(stream, (char*) &material_index_size, sizeof(uint8_t));
		ReadChecked(stream, (char*) &bone_index_size, sizeof(uint8_t));
		ReadChecked(stream, (char*) &morph_index_size, sizeof(uint8_t));
		ReadChecked(stream, (char*) &rigidbody_index_size, sizeof(uint8_t));
		auto validIndexSize = [](uint8_t size) { return size == 1 || size == 2 || size == 4; };
		if (encoding > 1 || uv > 4 || !validIndexSize(vertex_index_size) ||
			!validIndexSize(texture_index_size) || !validIndexSize(material_index_size) ||
			!validIndexSize(bone_index_size) || !validIndexSize(morph_index_size) ||
			!validIndexSize(rigidbody_index_size))
			throw std::runtime_error("Invalid PMX settings");
		uint8_t temp;
		for (int i = 8; i < count; i++)
		{
			ReadChecked(stream, (char*)&temp, sizeof(uint8_t));
		}
	}

	void PmxVertexSkinningBDEF1::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_index = ReadIndex(stream, setting->bone_index_size);
	}

	void PmxVertexSkinningBDEF2::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_index1 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index2 = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->bone_weight, sizeof(float));
	}

	void PmxVertexSkinningBDEF4::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_index1 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index2 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index3 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index4 = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->bone_weight1, sizeof(float));
		ReadChecked(stream, (char*) &this->bone_weight2, sizeof(float));
		ReadChecked(stream, (char*) &this->bone_weight3, sizeof(float));
		ReadChecked(stream, (char*) &this->bone_weight4, sizeof(float));
	}

	void PmxVertexSkinningSDEF::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_index1 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index2 = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->bone_weight, sizeof(float));
		ReadChecked(stream, (char*) this->sdef_c, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->sdef_r0, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->sdef_r1, sizeof(float) * 3);
	}

	void PmxVertexSkinningQDEF::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_index1 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index2 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index3 = ReadIndex(stream, setting->bone_index_size);
		this->bone_index4 = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->bone_weight1, sizeof(float));
		ReadChecked(stream, (char*) &this->bone_weight2, sizeof(float));
		ReadChecked(stream, (char*) &this->bone_weight3, sizeof(float));
		ReadChecked(stream, (char*) &this->bone_weight4, sizeof(float));
	}

	void PmxVertex::Read(std::istream *stream, PmxSetting *setting)
	{
		ReadChecked(stream, (char*) this->positon, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->normal, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->uv, sizeof(float) * 2);
		for (int i = 0; i < setting->uv; ++i)
		{
			ReadChecked(stream, (char*) this->uva[i], sizeof(float) * 4);
		}
		ReadChecked(stream, (char*) &this->skinning_type, sizeof(PmxVertexSkinningType));
		switch (this->skinning_type)
		{
		case PmxVertexSkinningType::BDEF1:
			this->skinning = std::make_unique<PmxVertexSkinningBDEF1>();
			break;
		case PmxVertexSkinningType::BDEF2:
			this->skinning = std::make_unique<PmxVertexSkinningBDEF2>();
			break;
		case PmxVertexSkinningType::BDEF4:
			this->skinning = std::make_unique<PmxVertexSkinningBDEF4>();
			break;
		case PmxVertexSkinningType::SDEF:
			this->skinning = std::make_unique<PmxVertexSkinningSDEF>();
			break;
		case PmxVertexSkinningType::QDEF:
			this->skinning = std::make_unique<PmxVertexSkinningQDEF>();
			break;
		default:
			throw std::runtime_error("Invalid PMX skinning type");
		}
		this->skinning->Read(stream, setting);
		ReadChecked(stream, (char*) &this->edge, sizeof(float));
	}

	void PmxMaterial::Read(std::istream *stream, PmxSetting *setting)
	{
		this->material_name = std::move(ReadString(stream, setting->encoding));
		this->material_english_name = std::move(ReadString(stream, setting->encoding));
		ReadChecked(stream, (char*) this->diffuse, sizeof(float) * 4);
		ReadChecked(stream, (char*) this->specular, sizeof(float) * 3);
		ReadChecked(stream, (char*) &this->specularlity, sizeof(float));
		ReadChecked(stream, (char*) this->ambient, sizeof(float) * 3);
		ReadChecked(stream, (char*) &this->flag, sizeof(uint8_t));
		ReadChecked(stream, (char*) this->edge_color, sizeof(float) * 4);
		ReadChecked(stream, (char*) &this->edge_size, sizeof(float));
		this->diffuse_texture_index = ReadIndex(stream, setting->texture_index_size);
		this->sphere_texture_index = ReadIndex(stream, setting->texture_index_size);
		ReadChecked(stream, (char*) &this->sphere_op_mode, sizeof(uint8_t));
		ReadChecked(stream, (char*) &this->common_toon_flag, sizeof(uint8_t));
		if (this->common_toon_flag)
		{
			ReadChecked(stream, (char*) &this->toon_texture_index, sizeof(uint8_t));
		}
		else {
			this->toon_texture_index = ReadIndex(stream, setting->texture_index_size);
		}
		this->memo = std::move(ReadString(stream, setting->encoding));
		ReadChecked(stream, (char*)&this->index_count, sizeof(int));
		if (this->index_count < 0) throw std::runtime_error("Negative material index count");
	}

	void PmxIkLink::Read(std::istream *stream, PmxSetting *setting)
	{
		this->link_target = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->angle_lock, sizeof(uint8_t));
		if (angle_lock == 1)
		{
			ReadChecked(stream, (char*) this->max_radian, sizeof(float) * 3);
			ReadChecked(stream, (char*) this->min_radian, sizeof(float) * 3);
		}
	}

	void PmxBone::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_name = std::move(ReadString(stream, setting->encoding));
		this->bone_english_name = std::move(ReadString(stream, setting->encoding));
		ReadChecked(stream, (char*) this->position, sizeof(float) * 3);
		this->parent_index = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->level, sizeof(int));
		ReadChecked(stream, (char*) &this->bone_flag, sizeof(uint16_t));
		if (this->bone_flag & 0x0001) {
			this->target_index = ReadIndex(stream, setting->bone_index_size);
		}
		else {
			ReadChecked(stream, (char*)this->offset, sizeof(float) * 3);
		}
		if (this->bone_flag & (0x0100 | 0x0200)) {
			this->grant_parent_index = ReadIndex(stream, setting->bone_index_size);
			ReadChecked(stream, (char*) &this->grant_weight, sizeof(float));
		}
		if (this->bone_flag & 0x0400) {
			ReadChecked(stream, (char*)this->lock_axis_orientation, sizeof(float) * 3);
		}
		if (this->bone_flag & 0x0800) {
			ReadChecked(stream, (char*)this->local_axis_x_orientation, sizeof(float) * 3);
			ReadChecked(stream, (char*)this->local_axis_y_orientation, sizeof(float) * 3);
		}
		if (this->bone_flag & 0x2000) {
			ReadChecked(stream, (char*) &this->key, sizeof(int));
		}
		if (this->bone_flag & 0x0020) {
			this->ik_target_bone_index = ReadIndex(stream, setting->bone_index_size);
			ReadChecked(stream, (char*) &ik_loop, sizeof(int));
			ReadChecked(stream, (char*) &ik_loop_angle_limit, sizeof(float));
			ReadCount(stream, ik_link_count);
			this->ik_links = std::make_unique<PmxIkLink []>(ik_link_count);
			for (int i = 0; i < ik_link_count; i++) {
				ik_links[i].Read(stream, setting);
			}
		}
	}

	void PmxMorphVertexOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->vertex_index = ReadIndex(stream, setting->vertex_index_size);
		ReadChecked(stream, (char*)this->position_offset, sizeof(float) * 3);
	}

	void PmxMorphUVOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->vertex_index = ReadIndex(stream, setting->vertex_index_size);
		ReadChecked(stream, (char*)this->uv_offset, sizeof(float) * 4);
	}

	void PmxMorphBoneOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->bone_index = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*)this->translation, sizeof(float) * 3);
		ReadChecked(stream, (char*)this->rotation, sizeof(float) * 4);
	}

	void PmxMorphMaterialOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->material_index = ReadIndex(stream, setting->material_index_size);
		ReadChecked(stream, (char*) &this->offset_operation, sizeof(uint8_t));
		ReadChecked(stream, (char*)this->diffuse, sizeof(float) * 4);
		ReadChecked(stream, (char*)this->specular, sizeof(float) * 3);
		ReadChecked(stream, (char*) &this->specularity, sizeof(float));
		ReadChecked(stream, (char*)this->ambient, sizeof(float) * 3);
		ReadChecked(stream, (char*)this->edge_color, sizeof(float) * 4);
		ReadChecked(stream, (char*) &this->edge_size, sizeof(float));
		ReadChecked(stream, (char*)this->texture_argb, sizeof(float) * 4);
		ReadChecked(stream, (char*)this->sphere_texture_argb, sizeof(float) * 4);
		ReadChecked(stream, (char*)this->toon_texture_argb, sizeof(float) * 4);
	}

	void PmxMorphGroupOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->morph_index = ReadIndex(stream, setting->morph_index_size);
		ReadChecked(stream, (char*) &this->morph_weight, sizeof(float));
	}

	void PmxMorphFlipOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->morph_index = ReadIndex(stream, setting->morph_index_size);
		ReadChecked(stream, (char*) &this->morph_value, sizeof(float));
	}

	void PmxMorphImpulseOffset::Read(std::istream *stream, PmxSetting *setting)
	{
		this->rigid_body_index = ReadIndex(stream, setting->rigidbody_index_size);
		ReadChecked(stream, (char*) &this->is_local, sizeof(uint8_t));
		ReadChecked(stream, (char*)this->velocity, sizeof(float) * 3);
		ReadChecked(stream, (char*)this->angular_torque, sizeof(float) * 3);
	}

	void PmxMorph::Read(std::istream *stream, PmxSetting *setting)
	{
		this->morph_name = ReadString(stream, setting->encoding);
		this->morph_english_name = ReadString(stream, setting->encoding);
		ReadChecked(stream, (char*) &category, sizeof(MorphCategory));
		ReadChecked(stream, (char*) &morph_type, sizeof(MorphType));
		ReadCount(stream, this->offset_count);
		switch (this->morph_type)
		{
		case MorphType::Group:
			group_offsets = std::make_unique<PmxMorphGroupOffset []>(this->offset_count);
			for (int i = 0; i < offset_count; i++)
			{
				group_offsets[i].Read(stream, setting);
			}
			break;
		case MorphType::Vertex:
			vertex_offsets = std::make_unique<PmxMorphVertexOffset []>(this->offset_count);
			for (int i = 0; i < offset_count; i++)
			{
				vertex_offsets[i].Read(stream, setting);
			}
			break;
		case MorphType::Bone:
			bone_offsets = std::make_unique<PmxMorphBoneOffset []>(this->offset_count);
			for (int i = 0; i < offset_count; i++)
			{
				bone_offsets[i].Read(stream, setting);
			}
			break;
		case MorphType::Matrial:
			material_offsets = std::make_unique<PmxMorphMaterialOffset []>(this->offset_count);
			for (int i = 0; i < offset_count; i++)
			{
				material_offsets[i].Read(stream, setting);
			}
			break;
		case MorphType::UV:
		case MorphType::AdditionalUV1:
		case MorphType::AdditionalUV2:
		case MorphType::AdditionalUV3:
		case MorphType::AdditionalUV4:
			uv_offsets = std::make_unique<PmxMorphUVOffset []>(this->offset_count);
			for (int i = 0; i < offset_count; i++)
			{
				uv_offsets[i].Read(stream, setting);
			}
			break;
		default:
			throw std::runtime_error("Invalid or unsupported PMX data");
		}
	}

	void PmxFrameElement::Read(std::istream *stream, PmxSetting *setting)
	{
		ReadChecked(stream, (char*) &this->element_target, sizeof(uint8_t));
		if (this->element_target == 0x00)
		{
			this->index = ReadIndex(stream, setting->bone_index_size);
		}
		else {
			this->index = ReadIndex(stream, setting->morph_index_size);
		}
	}

	void PmxFrame::Read(std::istream *stream, PmxSetting *setting)
	{
		this->frame_name = ReadString(stream, setting->encoding);
		this->frame_english_name = ReadString(stream, setting->encoding);
		ReadChecked(stream, (char*) &this->frame_flag, sizeof(uint8_t));
		ReadCount(stream, this->element_count);
		this->elements = std::make_unique<PmxFrameElement []>(this->element_count);
		for (int i = 0; i < this->element_count; i++)
		{
			this->elements[i].Read(stream, setting);
		}
	}

	void PmxRigidBody::Read(std::istream *stream, PmxSetting *setting)
	{
		this->rigid_body_name = ReadString(stream, setting->encoding);
		this->rigid_body_english_name = ReadString(stream, setting->encoding);
		this->target_bone = ReadIndex(stream, setting->bone_index_size);
		ReadChecked(stream, (char*) &this->group, sizeof(uint8_t));
		ReadChecked(stream, (char*) &this->mask, sizeof(uint16_t));
		ReadChecked(stream, (char*) &this->shape, sizeof(uint8_t));
		ReadChecked(stream, (char*) this->size, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->position, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->orientation, sizeof(float) * 3);
		ReadChecked(stream, (char*) &this->mass, sizeof(float));
		ReadChecked(stream, (char*) &this->move_attenuation, sizeof(float));
		ReadChecked(stream, (char*) &this->rotation_attenuation, sizeof(float));
		ReadChecked(stream, (char*) &this->repulsion, sizeof(float));
		ReadChecked(stream, (char*) &this->friction, sizeof(float));
		ReadChecked(stream, (char*) &this->physics_calc_type, sizeof(uint8_t));
	}

	void PmxJointParam::Read(std::istream *stream, PmxSetting *setting)
	{
		this->rigid_body1 = ReadIndex(stream, setting->rigidbody_index_size);
		this->rigid_body2 = ReadIndex(stream, setting->rigidbody_index_size);
		ReadChecked(stream, (char*) this->position, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->orientaiton, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->move_limitation_min, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->move_limitation_max, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->rotation_limitation_min, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->rotation_limitation_max, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->spring_move_coefficient, sizeof(float) * 3);
		ReadChecked(stream, (char*) this->spring_rotation_coefficient, sizeof(float) * 3);
	}

	void PmxJoint::Read(std::istream *stream, PmxSetting *setting)
	{
		this->joint_name = ReadString(stream, setting->encoding);
		this->joint_english_name = ReadString(stream, setting->encoding);
		ReadChecked(stream, (char*) &this->joint_type, sizeof(uint8_t));
		this->param.Read(stream, setting);
	}

	void PmxAnchorRigidBody::Read(std::istream *stream, PmxSetting *setting)
	{
		this->related_rigid_body = ReadIndex(stream, setting->rigidbody_index_size);
		this->related_vertex = ReadIndex(stream, setting->vertex_index_size);
		ReadChecked(stream, (char*) &this->is_near, sizeof(uint8_t));
	}

	void PmxSoftBody::Read(std::istream *stream, PmxSetting *setting)
	{
		// 未実装
		std::cerr << "Not Implemented Exception" << std::endl;
		throw std::runtime_error("Invalid or unsupported PMX data");
	}

	void PmxModel::Init()
	{
		this->version = 0.0f;
		this->model_name.clear();
		this->model_english_name.clear();
		this->model_comment.clear();
		this->model_english_comment.clear();
		this->vertex_count = 0;
		this->vertices = nullptr;
		this->index_count = 0;
		this->indices = nullptr;
		this->texture_count = 0;
		this->textures = nullptr;
		this->material_count = 0;
		this->materials = nullptr;
		this->bone_count = 0;
		this->bones = nullptr;
		this->morph_count = 0;
		this->morphs = nullptr;
		this->frame_count = 0;
		this->frames = nullptr;
		this->rigid_body_count = 0;
		this->rigid_bodies = nullptr;
		this->joint_count = 0;
		this->joints = nullptr;
		this->soft_body_count = 0;
		this->soft_bodies = nullptr;
	}

	void PmxModel::Read(std::istream *stream)
	{
		// マジック
		char magic[4];
		ReadChecked(stream, (char*) magic, sizeof(char) * 4);
		if (magic[0] != 0x50 || magic[1] != 0x4d || magic[2] != 0x58 || magic[3] != 0x20)
		{
			std::cerr << "invalid magic number." << std::endl;
			throw std::runtime_error("Invalid or unsupported PMX data");
		}
		// バージョン
		ReadChecked(stream, (char*) &version, sizeof(float));
		if (version != 2.0f && version != 2.1f)
		{
			std::cerr << "this is not ver2.0 or ver2.1 but " << version << "." << std::endl;
			throw std::runtime_error("Invalid or unsupported PMX data");
		}
		// ファイル設定
		this->setting.Read(stream);

		// モデル情報
		this->model_name = std::move(ReadString(stream, setting.encoding));
		this->model_english_name = std::move(ReadString(stream, setting.encoding));
		this->model_comment = std::move(ReadString(stream, setting.encoding));
		this->model_english_comment = std::move(ReadString(stream, setting.encoding));

		// 頂点
		ReadCount(stream, vertex_count, 37);
		this->vertices = std::make_unique<PmxVertex []>(vertex_count);
		for (int i = 0; i < vertex_count; i++)
		{
			vertices[i].Read(stream, &setting);
		}

		// 面
		ReadCount(stream, index_count, setting.vertex_index_size);
		this->indices = std::make_unique<int []>(index_count);
		for (int i = 0; i < index_count; i++)
		{
			this->indices[i] = ReadIndex(stream, setting.vertex_index_size);
		}

		// テクスチャ
		ReadCount(stream, texture_count, 4);
		this->textures = std::make_unique<utfstring []>(texture_count);
		for (int i = 0; i < texture_count; i++)
		{
			this->textures[i] = ReadString(stream, setting.encoding);
		}

		// マテリアル
		ReadCount(stream, material_count, 80);
		this->materials = std::make_unique<PmxMaterial []>(material_count);
		for (int i = 0; i < material_count; i++)
		{
			this->materials[i].Read(stream, &setting);
		}

		// ボーン
		ReadCount(stream, this->bone_count, 27);
		this->bones = std::make_unique<PmxBone []>(this->bone_count);
		for (int i = 0; i < this->bone_count; i++)
		{
			this->bones[i].Read(stream, &setting);
		}

		// モーフ
		ReadCount(stream, this->morph_count, 14);
		this->morphs = std::make_unique<PmxMorph []>(this->morph_count);
		for (int i = 0; i < this->morph_count; i++)
		{
			this->morphs[i].Read(stream, &setting);
		}

		// 表示枠
		ReadCount(stream, this->frame_count, 13);
		this->frames = std::make_unique<PmxFrame []>(this->frame_count);
		for (int i = 0; i < this->frame_count; i++)
		{
			this->frames[i].Read(stream, &setting);
		}

		// 剛体
		ReadCount(stream, this->rigid_body_count, 80);
		this->rigid_bodies = std::make_unique<PmxRigidBody []>(this->rigid_body_count);
		for (int i = 0; i < this->rigid_body_count; i++)
		{
			this->rigid_bodies[i].Read(stream, &setting);
		}

		// ジョイント
		ReadCount(stream, this->joint_count, 9);
		this->joints = std::make_unique<PmxJoint []>(this->joint_count);
		for (int i = 0; i < this->joint_count; i++)
		{
			this->joints[i].Read(stream, &setting);
		}

		//// ソフトボディ
		//if (this->version == 2.1f)
		//{
		//	ReadCount(stream, this->soft_body_count);
		//	this->soft_bodies = std::make_unique<PmxSoftBody []>(this->soft_body_count);
		//	for (int i = 0; i < this->soft_body_count; i++)
		//	{
		//		this->soft_bodies[i].Read(stream, &setting);
		//	}
		//}
	}

	//std::unique_ptr<PmxModel> ReadFromFile(const char *filename)
	//{
	//	auto stream = std::ifstream(filename, std::ios_base::binary);
	//	auto pmx = PmxModel::ReadFromStream(&stream);
	//	if (!stream.eof())
	//	{
	//		std::cerr << "don't reach the end of file." << std::endl;
	//	}
	//	stream.close();
	//	return pmx;
	//}

	//std::unique_ptr<PmxModel> ReadFromStream(std::istream *stream)
	//{
	//	auto pmx = std::make_unique<PmxModel>();
	//	pmx->Read(stream);
	//	return pmx;
	//}
}
