// improfx_control. RCSZ.
// CoreImGui, ISO_C11, ISO_C++17, MSVCx64.

#ifndef _IMPROFX_CONTROL_H
#define _IMPROFX_CONTROL_H
#include <functional>
#include <string>
#include <vector>
#include <cstdio>
#include <chrono>

#include "improfx_control_base.h"

namespace IMFXC_CWIN {
	
	// ######################################## SmoothMenu ChildWindow ########################################
	// PROJ: 2023_12_16 RCSZ. Update: 2024_04_20 RCSZ.

	// [ImGui�Ӵ���]: ƽ������˵�.
	class SmoothMenuChildWindow {
	protected:
		ImVec2 MenuBufferTypeScroll = {};
		ImVec2 MenuBufferWidthType  = {};

		ImVec2 MenuBufferItemScroll = {};
		ImVec2 MenuBufferWidthItem  = {};

		float TextDrawHeight = 0.0f;

		void DrawMenuTypeRect(float rect_height, const ImVec4& color);
		void DrawMenuItemRect(float rect_height, const ImVec4& color);

		void MenuInterCalc(ImVec2& posy_calc, ImVec2& width_calc, float speed);
	public:
		bool DrawMenuWindow(
			const char*                     name,
			const std::vector<std::string>& items,
			uint32_t&						count,
			const ImVec4&					color      = ImVec4(0.0f, 0.92f, 0.86f, 0.78f),
			const ImVec2&					size	   = ImVec2(256.0f, 384.0f),
			float                           speed	   = 1.0f,
			float                           text_scale = 1.2f
		);
	};

#ifdef ENABLE_OLD_CONTROL_DASHBOAR
	// ######################################## Dashboard ChildWindow ########################################
	// PROJ: 2024_02_16 RCSZ. Update: 2024_04_20 RCSZ.

	// [ImGui�Ӵ���]: �Ǳ�����ʾ.
	class DashboardChildWindow {
	protected:
		ImVec2 DashboardValueLimit = ImVec2(0.0f, 400.0f); // x:min, y:max
		ImVec2 DashboardValue      = {};                   // x:value, y:value_smooth.
		ImVec2 DashboardColorSub   = ImVec2(0.78f, 0.78f); // x:sub, y:sub_smooth.

		bool SelfInspStatusFlag = true;
		bool ValueSmoothFlag    = true;

		void DrawSemicircleBox(float window_width, float y_offset, uint32_t ruler, const ImVec4& color);
		void DrawRulerscaleValue(float window_width, float y_offset, uint32_t ruler, const ImVec2& limit, const ImVec4& color);
		void DrawIndicator(float window_width, float y_offset, float value, const ImVec2& limit, const ImVec4& color);
	public:
		bool DrawDashboardWindow(
			const char*   name,
			float         value,
			uint32_t      ruler = 10,
			const ImVec4& color = ImVec4(0.0f, 1.0f, 1.0f, 0.92f),
			const ImVec2& size  = ImVec2(768.0f, 768.0f),
			float         speed = 1.0f
		);

		ImVec2 SetDashboardValueLimit(const ImVec2& limit) {
			DashboardValueLimit = limit;
			DashboardValue.y = DashboardValueLimit.x;
			return DashboardValueLimit;
		}
		bool DashboardStart = false;
	};
#endif
}

namespace IMFXC_WIN {

	// ######################################## AnimAxisEditorWindow ########################################
	// PROJ: 2023_12_19 RCSZ. Update: 2024_05_30 RCSZ.

#define ANE_COORD_PARAMS 6 
	// animation points coordinates.
	struct AnimCoordSample {
		float AnimSamplePoints[ANE_COORD_PARAMS];
		float TimePosition;

		uint32_t PlayerSamplingRate;
		uint32_t BakeSamplingRate;

		AnimCoordSample() : AnimSamplePoints{}, TimePosition(0.0f), PlayerSamplingRate(128), BakeSamplingRate(256) {}
		AnimCoordSample(float x, float y, float z, float pt, float yw, float rl, float t, uint32_t ps = 128, uint32_t gs = 256) :
			AnimSamplePoints{ x, y, z, pt, yw, rl }, TimePosition(t), PlayerSamplingRate(ps), BakeSamplingRate(gs)
		{}
	};

	struct AnimGenCoord {
		float AnimGenVector[ANE_COORD_PARAMS];
	};

	// [ImGui����]: ������༭��.
	class AnimAxisEditorWindow {
	protected:
		// editor animation button.
		IM_CONTROL_BASE::IM_ANIM::ButtonAnim AnimButton[6] = {};
		// editor player_box const_color.
		const ImVec4 EditorColorPlayer = ImVec4(0.0f, 0.72f, 0.72f, 1.0f);

		float EditorScaleLinesWidth = 1.0f;
		// false:[x.y.z], true:[pitch.yaw.roll]
		bool EditorModeType = false;

		ImVec2 PlayerLineScrollx    = {}; // x:pos, y:smooth
		bool   PlayerSetScrollyFlag = false;
		bool   PlayerFlag           = false;

		ImVec2 TrackWindowScrollx = {};   // x:pos, y:max.
		float  TrackScrollx       = 0.0f; // tick inter_smooth.

		float* NodeSetValuePointer = nullptr;

		ImVec4      SystemAsixColors[3] = {};
		const char* SystemAsixTexts[3]  = {};

		AnimGenCoord PlayerRunCoord = {};
		std::vector<AnimCoordSample>* AnimDataIndex = nullptr;

		void MouseSetPointValue();
		void SpacingLine(float space_left, const ImVec4& color);

		void DrawCubicBezierCurve(
			const ImVec2& point0, const ImVec2& point1, const ImVec4& color, const ImVec2& scale,
			float offset, int sample, float centerh
		);
		void DrawAnimationPoints(const ImVec2& position, float size, const ImVec4& color, float& value);
		void DrawPlayerLine(ImVec2& position, float offset, float max, const ImVec4& color, float xscale);

		bool RunGetCubicBezierCurve(
			const ImVec2& point0, const ImVec2& point1, float& value, float playerpos, float centerh,
			std::vector<AnimCoordSample>& src, size_t index
		);
	public:
		AnimAxisEditorWindow();

		bool PlayerRunSample(AnimGenCoord& CoordParam);
		std::vector<AnimGenCoord> GenerateBakedBezierCurve();

		void DrawEditorWindow(
			uint32_t                      unqiue_id,
			const char*                   name,
			float                         track_length,
			float                         player_step,
			std::vector<AnimCoordSample>& sample,
			bool                          fixed_window = false,
			bool*                         p_open       = (bool*)0,
			ImGuiWindowFlags              flags        = NULL
		);

		float TrackWidthValueScale  = 1.0f;
		float TrackHeightValueScale = 1.0f;

		// window global colorsystem.
		ImVec4 EditorColorSystem = ImVec4(0.86f, 0.86f, 0.86f, 0.94f);
	};

	// ######################################## FlatEditorWindow ########################################
	// PROJ: 2023_12_25 RCSZ. Update: 2024_02_16 RCSZ.

	// editor coordinate_system information.
	struct CoordSystemInfo {
		ImVec2 CenterPosition;
		ImVec2 MouseCoordPos;

		ImVec2 SelectionBoxPoint0;
		ImVec2 SelectionBoxPoint1;

		float ScaleCoord;

		CoordSystemInfo() : CenterPosition({}), MouseCoordPos({}), SelectionBoxPoint0({}), SelectionBoxPoint1({}), ScaleCoord(1.0f) {}
		CoordSystemInfo(const ImVec2& cen, const ImVec2& pos, const ImVec2& bxp0, const ImVec2& bxp1, float scale) :
			CenterPosition(cen), MouseCoordPos(pos), SelectionBoxPoint0(bxp0), SelectionBoxPoint1(bxp1), ScaleCoord(scale)
		{}
	};

	// [ImGui����]: 2Dƽ��༭��.
	class FlatEditorWindow {
	protected:
		CoordSystemInfo EditorCoordInfo = {};

		ImVec2 GridCenterPosition       = {};
		ImVec2 GridCenterPositionSmooth = {};

		ImVec2 GridSizeScale     = ImVec2(1.0f, 1.0f); // x:scale, y:smooth
		ImVec2 GridMousePosition = {};

		ImVec2 EditorSeleBoxWinPoints[2] = {};
		ImVec2 EditorSeleBoxVirPoints[2] = {};

		ImVec2 PosWinCurrPosition = {};

		bool GridWinFocus    = false;
		bool PosWinCurrFocus = false;

		void DrawCoordinateXRuler(const ImVec2& limit, const ImVec4& color, float length, float center, float scale, float ruler = 20.0f);
		void DrawCoordinateYRuler(const ImVec2& limit, const ImVec4& color, float length, float center, float scale, float ruler = 20.0f);
		void DrawCoordinateLines(const ImVec4& color, const ImVec2& mouse, const ImVec2& win_size);

		void DrawGrid(
			const ImVec2& limitx, const ImVec2& limity, const ImVec4& color, const ImVec2& pos, const ImVec2& win_size,
			float scale, float ruler = 20.0f
		);
		// child window.
		void PositioningWindow(
			ImVec2& index, const ImVec2& limitx, const ImVec2& limity, const ImVec4& color, ImTextureID texture,
			float size, float scale
		);
	public:
		void DrawEditorWindow(
			uint32_t							 unqiue_id,
			const char*                          name,
			std::function<void(CoordSystemInfo)> draw,
			ImTextureID                          poswin_image  = 0,
			bool								 fixed_window  = true,
			float								 speed         = 1.0f,
			const ImVec2&                        coord_size    = ImVec2(1200.0f, 1200.0f),
			const ImVec2&                        coord_winsize = ImVec2(640.0f, 640.0f),
			bool*                                p_open        = (bool*)0,
			ImGuiWindowFlags                     flags         = NULL
		);

		// toolbar custom control func. 
		std::function<void()> EditorToolbar = []() {};

		// window global colorsystem.
		ImVec4 EditorColorSystem = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
	};

	// ######################################## FlatEditorWindow ########################################
	// PROJ: 2024_05_30 RCSZ. Update: 2024_05_30 RCSZ.

	struct ShortcutKeyInfo {
		int MainKeyIndex;
		std::string           KeyShortcutName;
		std::vector<ImGuiKey> KeyCombination;

		ShortcutKeyInfo() : MainKeyIndex(NULL), KeyShortcutName(""), KeyCombination({}) {}
	};

	// [ImGui����]: ��Ͽ�ݼ��༭��.
	class ShortcutKeyEditorWindow {
	protected:
		std::vector<ShortcutKeyInfo> ShortcutKeys = {};
		std::chrono::steady_clock::time_point CombinationKeyTimer = {};
		std::vector<ImGuiKey> CombinationKey = {};

		int64_t CombinationKeyDuration = NULL;
		bool    CombinationKeyFlag     = {};

		ImGuiKey PressWhichKey(bool repeat = true);
		// keys == CombinationKey ?
		bool IfCombinationKey(const std::vector<ImGuiKey>& keys);
	public:
		std::vector<ShortcutKeyInfo>* GetSourceData() {
			return &ShortcutKeys;
		}
		int32_t LAST_HIT = -1;
		int32_t UpdateShortcutKey();

		void DrawEditorWindow(
			uint32_t         unqiue_id,
			const char*      name,
			float            interval,
			bool             fixed_window = false,
			bool*            p_open       = (bool*)0,
			ImGuiWindowFlags flags        = NULL
		);

		// shortcut_key name text(char) length.
		size_t KeyNameLength = 256;

		// window global colorsystem.
		ImVec4 EditorColorSystem = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
	};
}

#endif