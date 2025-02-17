// improfx_control_dashboard. RCSZ. [20240216]
// ImGui: [ChildWindow(BeginChild_EndChild)], Aashboard View, Update: 20240216.
#include "improfx_control.h"

constexpr ImVec2 SemicircleDeg = ImVec2(45.0f, 315.0f);
constexpr ImVec2 RulerScaleLen = ImVec2(3.0f, 1.2f);
constexpr float IndicatorWidth = 7.2f;

inline void LEDSTATE(bool STAT, const ImVec2& POS, const ImVec4& HCOLOR, const ImVec4& LCOLOR) {
	if (STAT) IM_CONTROL_BASE::ListDrawRectangleFill(POS, ImVec2(40.0f, 20.0f), HCOLOR);
	else      IM_CONTROL_BASE::ListDrawRectangleFill(POS, ImVec2(40.0f, 20.0f), LCOLOR);
}

namespace IMFXC_CWIN {
#ifdef OLD_CONTROL_DASHBOAR
	void DashboardChildWindow::DrawSemicircleBox(float window_width, float y_offset, uint32_t ruler, const ImVec4& color) {
		float DrawOffsetHigh = window_width * 0.5f;
		ImVec2 CircleRadius = ImVec2(DrawOffsetHigh - IMGUI_ITEM_SPAC, window_width * 0.32f);

		ImVec2 LineBeginInnerTemp = ImVec2(
			sin(IMFXC_DEGTORAD(SemicircleDeg.x)) * CircleRadius.x + DrawOffsetHigh,
			cos(IMFXC_DEGTORAD(SemicircleDeg.x)) * CircleRadius.x + y_offset
		);

		ImVec2 RulerDrawSpc = ImVec2(
			(SemicircleDeg.y - SemicircleDeg.x) / (float)ruler,
			(SemicircleDeg.y - SemicircleDeg.x) / float(ruler * 5)
		);
		ImVec2 RulerDrawCount = ImVec2(0.0f, 1.0f);

		// draw dashboard ruler_scale.
		for (float i = SemicircleDeg.x; i < SemicircleDeg.y; i += 0.15f) {

			ImVec2 LineEndInnerTemp = ImVec2(
				sin(IMFXC_DEGTORAD(i)) * CircleRadius.x + DrawOffsetHigh,
				cos(IMFXC_DEGTORAD(i)) * CircleRadius.x + y_offset
			);

			if (i - SemicircleDeg.x >= RulerDrawSpc.x * RulerDrawCount.x) {
				// draw main ruler_scale.
				ImVec2 LineRulerTemp = ImVec2(
					sin(IMFXC_DEGTORAD(i)) * (CircleRadius.x - IMGUI_ITEM_SPAC * RulerScaleLen.x) + DrawOffsetHigh,
					cos(IMFXC_DEGTORAD(i)) * (CircleRadius.x - IMGUI_ITEM_SPAC * RulerScaleLen.x) + y_offset
				);
				IM_CONTROL_BASE::ListDrawLine(LineEndInnerTemp, LineRulerTemp, color, 4.0f);
				RulerDrawCount.x += 1.0f;
			}

			if (i - SemicircleDeg.x > RulerDrawSpc.y * RulerDrawCount.y) {
				// draw small ruler_scale.
				ImVec2 LineRulerTemp = ImVec2(
					sin(IMFXC_DEGTORAD(i)) * (CircleRadius.x - IMGUI_ITEM_SPAC * RulerScaleLen.y) + DrawOffsetHigh,
					cos(IMFXC_DEGTORAD(i)) * (CircleRadius.x - IMGUI_ITEM_SPAC * RulerScaleLen.y) + y_offset
				);
				IM_CONTROL_BASE::ListDrawLine(LineEndInnerTemp, LineRulerTemp, color, 2.2f);
				RulerDrawCount.y += 1.0f;
			}

			IM_CONTROL_BASE::ListDrawLine(LineBeginInnerTemp, LineEndInnerTemp, color, 4.8f);
			LineBeginInnerTemp = LineEndInnerTemp;
		}

		ImVec2 LineBeginOuterTemp = ImVec2(
			sin(IMFXC_DEGTORAD(SemicircleDeg.x + 20.0f)) * CircleRadius.y + DrawOffsetHigh,
			cos(IMFXC_DEGTORAD(SemicircleDeg.x + 20.0f)) * CircleRadius.y + y_offset
		);

		// draw dashboard outer_circle.
		for (float i = SemicircleDeg.x + 20.0f; i < SemicircleDeg.y - 20.0f; i += 0.15f) {

			ImVec2 LineEndOuterTemp = ImVec2(
				sin(IMFXC_DEGTORAD(i)) * CircleRadius.y + DrawOffsetHigh,
				cos(IMFXC_DEGTORAD(i)) * CircleRadius.y + y_offset
			);
			IM_CONTROL_BASE::ListDrawLine(LineBeginOuterTemp, LineEndOuterTemp, color, 16.0f);
			LineBeginOuterTemp = LineEndOuterTemp;
		}

		// end ruler_scale.
		IM_CONTROL_BASE::ListDrawLine(
			ImVec2(
				sin(IMFXC_DEGTORAD(SemicircleDeg.y)) * CircleRadius.x + DrawOffsetHigh,
				cos(IMFXC_DEGTORAD(SemicircleDeg.y)) * CircleRadius.x + y_offset),
			ImVec2(
				sin(IMFXC_DEGTORAD(SemicircleDeg.y)) * (CircleRadius.x - IMGUI_ITEM_SPAC * RulerScaleLen.x) + DrawOffsetHigh,
				cos(IMFXC_DEGTORAD(SemicircleDeg.y)) * (CircleRadius.x - IMGUI_ITEM_SPAC * RulerScaleLen.x) + y_offset),
			color, 2.2f
		);
	}

	void DashboardChildWindow::DrawRulerscaleValue(float window_width, float y_offset, uint32_t ruler, const ImVec2& limit, const ImVec4& color) {
		float DrawOffsetHigh = window_width * 0.5f;
		float CircleRadius = DrawOffsetHigh - IMGUI_ITEM_SPAC;
		float RulerDrawSpc = (SemicircleDeg.y - SemicircleDeg.x) / (float)ruler;

		float RulerDrawCount = 0.0f, ValueDrawTemp = limit.y;
		float ValueOffset = (limit.y - limit.x) / (float)ruler;

		auto DrawTextFunc = [&](const char* DrawText, const ImVec2& DrawPos) {
			// imgui draw offset_text.
			ImVec2 TextSizeOffset = ImGui::CalcTextSize(DrawText);
			IM_CONTROL_BASE::ListDrawText(ImVec2(DrawPos.x - TextSizeOffset.x * 0.5f, DrawPos.y - TextSizeOffset.y * 0.5f), color, DrawText);
		};

		std::string DrawTextTemp = {};
		// draw dashboard ruler_value.
		for (float i = SemicircleDeg.x; i < SemicircleDeg.y; i += 0.32f) {

			if (i - SemicircleDeg.x >= RulerDrawSpc * RulerDrawCount) {
				// draw main ruler_value.
				ImVec2 DrawTextPos = ImVec2(
					sin(IMFXC_DEGTORAD(i)) * (CircleRadius - IMGUI_ITEM_SPAC * RulerScaleLen.x * 2.4f) + DrawOffsetHigh,
					cos(IMFXC_DEGTORAD(i)) * (CircleRadius - IMGUI_ITEM_SPAC * RulerScaleLen.x * 2.4f) + y_offset
				);
				DrawTextTemp = std::to_string(int32_t(ValueDrawTemp));
				ValueDrawTemp -= ValueOffset;

				DrawTextFunc(DrawTextTemp.c_str(), DrawTextPos);
				RulerDrawCount += 1.0f;
			}
		}

		// draw head ruler_value.
		ImVec2 HeadTextPos = ImVec2(
			sin(IMFXC_DEGTORAD(SemicircleDeg.y)) * (CircleRadius - IMGUI_ITEM_SPAC * RulerScaleLen.x * 2.4f) + DrawOffsetHigh,
			cos(IMFXC_DEGTORAD(SemicircleDeg.y)) * (CircleRadius - IMGUI_ITEM_SPAC * RulerScaleLen.x * 2.4f) + y_offset
		);
		DrawTextFunc(std::to_string((int32_t)limit.x).c_str(), HeadTextPos);
	}

	void DashboardChildWindow::DrawIndicator(float window_width, float y_offset, float value, const ImVec2& limit, const ImVec4& color) {
		float DrawOffsetHigh = window_width * 0.5f;
		// ָ���ȽǶ�����.
		float IWL = IndicatorWidth / (DrawOffsetHigh * 0.85f);

		float SemicircleDegLen = SemicircleDeg.y - SemicircleDeg.x + IWL * 180.0f;
		float ValueLength = limit.y - limit.x;
		float ValueProportion = (value - limit.x) / ValueLength;

		ValueProportion = IMFXC_V_CLAMP(ValueProportion, 0.0f, 1.0f);

		ImVec2 CenterPoint = ImVec2(window_width * 0.5f, y_offset);
		ImVec2 LineBeginTemp = ImVec2(
			sin(IMFXC_DEGTORAD(SemicircleDeg.y - SemicircleDegLen * ValueProportion) + IWL) * DrawOffsetHigh * 0.85f - IMGUI_ITEM_SPAC + DrawOffsetHigh,
			cos(IMFXC_DEGTORAD(SemicircleDeg.y - SemicircleDegLen * ValueProportion) + IWL) * DrawOffsetHigh * 0.85f - IMGUI_ITEM_SPAC + y_offset
		);

		// �Ǳ�ָ����� => �Ǳ�ָ�� => ָ���.
		IM_CONTROL_BASE::ListDrawCircleFill(CenterPoint, 32.0f, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		IM_CONTROL_BASE::ListDrawLine(LineBeginTemp, CenterPoint, ImVec4(1.0f, 0.0f, 0.0f, 0.98f), IndicatorWidth);
		IM_CONTROL_BASE::ListDrawCircleFill(CenterPoint, 24.0f, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
	}

	bool DashboardChildWindow::DrawDashboardWindow(
		const char*   name, 
		float         value,
		uint32_t      ruler,
		const ImVec4& color,
		const ImVec2& size,
		float         speed
	) {
		if (!SelfInspStatusFlag && DashboardStart)
			DashboardValue.x = value;
		DashboardValue.x = IMFXC_V_CLAMP(DashboardValue.x, DashboardValueLimit.x, DashboardValueLimit.y);

		// value_limit: min > max.
		if (DashboardValueLimit.x >= DashboardValueLimit.y)
			DashboardValueLimit.x = DashboardValueLimit.y - 1.0f;

		ImGui::BeginChild(name, size, true);
		ImGui::SetWindowFontScale(1.42f);

		if (DashboardStart) {
			// �Ǳ����Լ�(ģ��).
			if (SelfInspStatusFlag && DashboardValue.x < DashboardValueLimit.x + 1.0f) {
				ValueSmoothFlag = true;
				DashboardValue.x = DashboardValueLimit.y;
			}
			if (SelfInspStatusFlag && (DashboardValue.x - DashboardValue.y) < DashboardValue.x * 0.08f) {
				SelfInspStatusFlag = false;
				DashboardValue.x = DashboardValueLimit.x;
			}
			// ƽ����ֵ����ر�.
			if (abs(DashboardValue.x - DashboardValue.y) < 0.25f)
				ValueSmoothFlag = false;
			DashboardColorSub.x = 0.0f;
		}
		else {
			DashboardColorSub.x = 0.78f;
			DashboardValue.x = DashboardValueLimit.x;
			// �Ǳ�ر�ָ���λ����.
			ValueSmoothFlag = true;
			SelfInspStatusFlag = true;
		}

		LEDSTATE(
			DashboardStart && SelfInspStatusFlag, 
			ImVec2(IMGUI_ITEM_SPAC, IMGUI_ITEM_SPAC), 
			ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 
			IM_CONTROL_BASE::ColorBrightnesScale(color, 0.78f)
		);
		LEDSTATE(
			DashboardStart && !SelfInspStatusFlag, 
			ImVec2(40.0f + IMGUI_ITEM_SPAC * 2.0f, IMGUI_ITEM_SPAC),
			ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
			IM_CONTROL_BASE::ColorBrightnesScale(color, 0.78f)
		);

		ImGui::SetNextWindowPos(ImVec2(
			ImGui::GetWindowPos().x + size.x * 0.5f - size.x * 0.16f,
			ImGui::GetWindowPos().y + size.y * 0.725f
		));
		// value dis window.
		ImVec2 DisWinSizeTemp = ImVec2(size.x * 0.32f, size.y * 0.08f);

		// �Ӵ����ڻ������ȼ�����, ���Ծ��α�����������.
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
		IM_CONTROL_BASE::ListDrawRectangleFill(
			ImVec2(size.x * 0.5f - size.x * 0.16f, size.y * 0.725f),
			DisWinSizeTemp, 
			IM_CONTROL_BASE::ColorBrightnesScale(color, 0.58f)
		);
		ImGui::PopStyleVar();

		ImGui::BeginChild(name, DisWinSizeTemp);
		ImGui::SetWindowFontScale(1.72f);
		{
			char ValueTextBuffer[8] = {};
			sprintf_s(ValueTextBuffer, "%.2f", value);

			ImVec2 TextWidth = ImGui::CalcTextSize(ValueTextBuffer);
			ImVec2 DrawTextPos = ImVec2(DisWinSizeTemp.x * 0.5f - TextWidth.x * 0.5f, 0.0f);
			if (DashboardStart)
				IM_CONTROL_BASE::ListDrawText(
					DrawTextPos, 
					IM_CONTROL_BASE::ColorBrightnesScale(color, DashboardColorSub.y), 
					"%0.2f", value
				);
		}
		ImGui::EndChild();

		DrawSemicircleBox(size.x, size.y * 0.6f, ruler,IM_CONTROL_BASE::ColorBrightnesScale(color, DashboardColorSub.y));
		DrawRulerscaleValue(size.x, size.y * 0.6f, ruler, DashboardValueLimit, IM_CONTROL_BASE::ColorBrightnesScale(color, DashboardColorSub.y));
		DrawIndicator(size.x, size.y * 0.6f, DashboardValue.y, DashboardValueLimit, color);
		
		// color & value inter_calc.
		DashboardColorSub.y += (DashboardColorSub.x - DashboardColorSub.y) * 0.024f * speed;
		if (ValueSmoothFlag)
			DashboardValue.y += (DashboardValue.x - DashboardValue.y) * 0.016f * speed;
		else
			DashboardValue.y = DashboardValue.x;

		ImGui::EndChild();
		return ImGui::IsWindowHovered();
	}
#endif
}