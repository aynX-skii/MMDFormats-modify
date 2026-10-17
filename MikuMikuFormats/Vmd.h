#pragma once

#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <ostream>
#include <cstring>
#include <charconv>
#include <stdexcept>
#include <cstdint>

namespace vmd
{
  inline void ReadChecked(std::istream* stream, char* data, std::streamsize size) {
    if (!stream || size < 0 || !stream->read(data, size)) throw std::runtime_error("Truncated VMD data");
  }
  inline void ReadCount(std::istream* stream, int& count, size_t recordBytes) {
    ReadChecked(stream, reinterpret_cast<char*>(&count), sizeof(count));
    std::streamoff available = -1;
    const auto position = stream->tellg();
    if (position != std::istream::pos_type(-1)) {
      stream->seekg(0, std::ios::end);
      const auto end = stream->tellg();
      stream->clear(); stream->seekg(position);
      if (end != std::istream::pos_type(-1)) available = end - position;
    } else {
      stream->clear(); available = stream->rdbuf()->in_avail();
    }
    if (count < 0 || count > 16000000 ||
        (available >= 0 && uint64_t(count) * recordBytes > uint64_t(available)))
      throw std::runtime_error("Invalid VMD element count");
  }

	/// ボーンフレーム
	class VmdBoneFrame
	{
	public:
		/// ボーン名
		std::string name;
		/// フレーム番号
		int frame;
		/// 位置
		float position[3];
		/// 回転
		float orientation[4];
		/// 補間曲線
		char interpolation[4][4][4];

		void Read(std::istream* stream)
		{
			char buffer[15];
			ReadChecked(stream, buffer, 15);
			size_t len = 0;
			while (len < 15 && buffer[len] != '\0') ++len;
			name = std::string(buffer, len);
			ReadChecked(stream, (char*) &frame, sizeof(int));
			ReadChecked(stream, (char*) position, sizeof(float)*3);
			ReadChecked(stream, (char*) orientation, sizeof(float)*4);
			ReadChecked(stream, (char*) interpolation, sizeof(char) * 4 * 4 * 4);
		}

		void Write(std::ostream* stream)
		{
			char buffer[15] = {0};
			const size_t copyLen = (name.size() < 15) ? name.size() : 15;
			if (copyLen > 0) {
				memcpy(buffer, name.data(), copyLen);
			}
			stream->write(buffer, 15);
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)position, sizeof(float) * 3);
			stream->write((char*)orientation, sizeof(float) * 4);
			stream->write((char*)interpolation, sizeof(char) * 4 * 4 * 4);
		}
	};

	/// 表情フレーム
	class VmdFaceFrame
	{
	public:
		/// 表情名
		std::string face_name;
		/// 表情の重み
		float weight;
		/// フレーム番号
		int32_t frame;

		void Read(std::istream* stream)
		{
			char buffer[15];
			ReadChecked(stream, buffer, 15);
			size_t len = 0;
			while (len < 15 && buffer[len] != '\0') ++len;
			face_name = std::string(buffer, len);
			ReadChecked(stream, (char*) &frame, sizeof(int));
			ReadChecked(stream, (char*) &weight, sizeof(float));
		}

		void Write(std::ostream* stream)
		{
			char buffer[15] = {0};
			const size_t copyLen = (face_name.size() < 15) ? face_name.size() : 15;
			if (copyLen > 0) {
				memcpy(buffer, face_name.data(), copyLen);
			}
			stream->write(buffer, 15);
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)&weight, sizeof(float));
		}
	};

	/// カメラフレーム
	class VmdCameraFrame
	{
	public:
		/// フレーム番号
		int frame;
		/// 距離
		float distance;
		/// 位置
		float position[3];
		/// 回転
		float orientation[3];
		/// 補間曲線
		char interpolation[6][4];
		/// 視野角 (整数度数, 1~180)
		int angle;
		/// パースペクティブフラグ (0 = on, 1 = off)
		char unknown[1];

		void Read(std::istream *stream)
		{
			ReadChecked(stream, (char*) &frame, sizeof(int));
			ReadChecked(stream, (char*) &distance, sizeof(float));
			ReadChecked(stream, (char*) position, sizeof(float) * 3);
			ReadChecked(stream, (char*) orientation, sizeof(float) * 3);
			ReadChecked(stream, (char*) interpolation, sizeof(char) * 24);
			ReadChecked(stream, (char*) &angle, sizeof(int));
			ReadChecked(stream, (char*) unknown, sizeof(char) * 1);
		}

		void Write(std::ostream *stream)
		{
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)&distance, sizeof(float));
			stream->write((char*)position, sizeof(float) * 3);
			stream->write((char*)orientation, sizeof(float) * 3);
			stream->write((char*)interpolation, sizeof(char) * 24);
			stream->write((char*)&angle, sizeof(int));
			stream->write((char*)unknown, sizeof(char) * 1);
		}
	};

	/// ライトフレーム
	class VmdLightFrame
	{
	public:
		/// フレーム番号
		int frame;
		/// 色
		float color[3];
		/// 位置
		float position[3];

		void Read(std::istream* stream)
		{
			ReadChecked(stream, (char*) &frame, sizeof(int));
			ReadChecked(stream, (char*) color, sizeof(float) * 3);
			ReadChecked(stream, (char*) position, sizeof(float) * 3);
		}

		void Write(std::ostream* stream)
		{
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)color, sizeof(float) * 3);
			stream->write((char*)position, sizeof(float) * 3);
		}
	};

	/// IKの有効無効
	class VmdIkEnable
	{
	public:
		std::string ik_name;
		bool enable;
	};

	/// IKフレーム
	class VmdIkFrame
	{
	public:
		int frame;
		bool display;
		std::vector<VmdIkEnable> ik_enable;

		void Read(std::istream *stream)
		{
			char buffer[20];
			ReadChecked(stream, (char*) &frame, sizeof(int));
			ReadChecked(stream, (char*) &display, sizeof(uint8_t));
			int ik_count;
			ReadCount(stream, ik_count, 21);
			ik_enable.resize(ik_count);
			for (int i = 0; i < ik_count; i++)
			{
				ReadChecked(stream, buffer, 20);
				size_t len = 0;
				while (len < 20 && buffer[len] != '\0') ++len;
				ik_enable[i].ik_name = std::string(buffer, len);
				ReadChecked(stream, (char*) &ik_enable[i].enable, sizeof(uint8_t));
			}
		}

		void Write(std::ostream *stream)
		{
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)&display, sizeof(uint8_t));
			int ik_count = static_cast<int>(ik_enable.size());
			stream->write((char*)&ik_count, sizeof(int));
			for (int i = 0; i < ik_count; i++)
			{
				const VmdIkEnable& ik_enable = this->ik_enable.at(i);
				char buffer[20] = {0};
				const size_t copyLen = (ik_enable.ik_name.size() < 20) ? ik_enable.ik_name.size() : 20;
				if (copyLen > 0) {
					memcpy(buffer, ik_enable.ik_name.data(), copyLen);
				}
				stream->write(buffer, 20);
				stream->write((char*)&ik_enable.enable, sizeof(uint8_t));
			}
		}
	};

	/// VMDモーション
	class VmdMotion
	{
	public:
		/// モデル名
		std::string model_name;
		/// バージョン
		int version;
		/// ボーンフレーム
		std::vector<VmdBoneFrame> bone_frames;
		/// 表情フレーム
		std::vector<VmdFaceFrame> face_frames;
		/// カメラフレーム
		std::vector<VmdCameraFrame> camera_frames;
		/// ライトフレーム
		std::vector<VmdLightFrame> light_frames;
		/// IKフレーム
		std::vector<VmdIkFrame> ik_frames;

		static std::unique_ptr<VmdMotion> LoadFromFile(char const *filename)
		{
			std::ifstream stream(filename, std::ios::binary);
			auto result = LoadFromStream(&stream);
			stream.close();
			return result;
		}

		static std::unique_ptr<VmdMotion> LoadFromStream(std::istream *stream)
		{

			char buffer[31] = {};
			auto result = std::make_unique<VmdMotion>();

			// magic and version
			ReadChecked(stream, (char*) buffer, 30);
			if (strncmp(buffer, "Vocaloid Motion Data", 20))
			{
				std::cerr << "invalid vmd file." << std::endl;
				return nullptr;
			}
			const char* version = buffer + 20;
			while (*version == ' ') ++version;
			if (std::from_chars(version, buffer + 30, result->version).ec != std::errc{} || result->version != 2)
				throw std::runtime_error("Unsupported VMD version");

			// name
			ReadChecked(stream, buffer, 20);
			size_t len = 0;
			while (len < 20 && buffer[len] != '\0') ++len;
			result->model_name = std::string(buffer, len);

			// bone frames
			int bone_frame_num;
			ReadCount(stream, bone_frame_num, 111);
			result->bone_frames.resize(bone_frame_num);
			for (int i = 0; i < bone_frame_num; i++)
			{
				result->bone_frames[i].Read(stream);
			}

			// face frames
			int face_frame_num;
			ReadCount(stream, face_frame_num, 23);
			result->face_frames.resize(face_frame_num);
			for (int i = 0; i < face_frame_num; i++)
			{
				result->face_frames[i].Read(stream);
			}

			// camera frames
			int camera_frame_num;
			ReadCount(stream, camera_frame_num, 61);
			result->camera_frames.resize(camera_frame_num);
			for (int i = 0; i < camera_frame_num; i++)
			{
				result->camera_frames[i].Read(stream);
			}

			// light frames
			int light_frame_num;
			ReadCount(stream, light_frame_num, 28);
			result->light_frames.resize(light_frame_num);
			for (int i = 0; i < light_frame_num; i++)
			{
				result->light_frames[i].Read(stream);
			}

			// Optional self-shadow and IK sections. Older motions may end here.
      if (stream->peek() == std::char_traits<char>::eof()) return result;
      int shadowCount = 0;
      ReadCount(stream, shadowCount, 9);
      for (int i=0;i<shadowCount;++i) ReadChecked(stream, buffer, 9);

			// ik frames
			if (stream->peek() != std::ios::traits_type::eof())
			{
				int ik_num;
				ReadCount(stream, ik_num, 9);
				result->ik_frames.resize(ik_num);
				for (int i = 0; i < ik_num; i++)
				{
					result->ik_frames[i].Read(stream);
				}
			}

			if (stream->peek() != std::ios::traits_type::eof())
			{
				std::cerr << "vmd stream has unknown data." << std::endl;
			}

			return result;
		}

		bool SaveToFile(const std::string& filename)
		{
			std::ofstream stream(filename, std::ios::binary);
			auto result = SaveToStream(&stream);
			stream.close();
			return result;
		}

		bool SaveToFile(const std::u16string& filename)
		{
			std::u16string fn = filename;
			std::wstring wfn(fn.begin(), fn.end());
			std::ofstream stream(wfn, std::ios::binary);
			auto result = SaveToStream(&stream);
			stream.close();
			return result;
		}

		bool SaveToStream(std::ostream *stream)
		{
			std::string magic = "Vocaloid Motion Data 0002\0";
			magic.resize(30);

			// magic and version
			stream->write(magic.c_str(), 30);

			// name
			char nameBuf[20] = {0};
			const size_t copyLen = (model_name.size() < 20) ? model_name.size() : 20;
			if (copyLen > 0) {
				memcpy(nameBuf, model_name.data(), copyLen);
			}
			stream->write(nameBuf, 20);

			// bone frames
			const int bone_frame_num = static_cast<int>(bone_frames.size());
			stream->write(reinterpret_cast<const char*>(&bone_frame_num), sizeof(int));
			for (int i = 0; i < bone_frame_num; i++)
			{
				bone_frames[i].Write(stream);
			}

			// face frames
			const int face_frame_num = static_cast<int>(face_frames.size());
			stream->write(reinterpret_cast<const char*>(&face_frame_num), sizeof(int));
			for (int i = 0; i < face_frame_num; i++)
			{
				face_frames[i].Write(stream);
			}

			// camera frames
			const int camera_frame_num = static_cast<int>(camera_frames.size());
			stream->write(reinterpret_cast<const char*>(&camera_frame_num), sizeof(int));
			for (int i = 0; i < camera_frame_num; i++)
			{
				camera_frames[i].Write(stream);
			}

			// light frames
			const int light_frame_num = static_cast<int>(light_frames.size());
			stream->write(reinterpret_cast<const char*>(&light_frame_num), sizeof(int));
			for (int i = 0; i < light_frame_num; i++)
			{
				light_frames[i].Write(stream);
			}

			// self shadow datas
			const int self_shadow_num = 0;
			stream->write(reinterpret_cast<const char*>(&self_shadow_num), sizeof(int));

			// ik frames
			const int ik_num = static_cast<int>(ik_frames.size());
			stream->write(reinterpret_cast<const char*>(&ik_num), sizeof(int));
			for (int i = 0; i < ik_num; i++)
			{
				ik_frames[i].Write(stream);
			}

			return true;
		}
	};
}