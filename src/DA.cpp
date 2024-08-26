#include "DA.hpp"

namespace DigitalAtavism {

float quantize(float value, unsigned bits)
{
	if (bits == 0)
	{
		return 0.f;
	}
	else if (bits == 1)
	{
		return value >= 0.f ? 1.f : -1.f;
	}
	else if (bits > 64)
	{
		return value;
	}
	else
	{
		const std::int64_t max = (1 << (bits - 1)) - 1;
		return static_cast<float>(static_cast<std::int64_t>(value * max)) / max;
	}
}

namespace gui {

void BaseModuleWidget::draw(const DrawArgs& args)
{
	{
		nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
		nvgFillColor(args.vg, nvgRGB(0xc9, 0xc9, 0xc9));

		nvgStrokeWidth(args.vg, 1);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, RACK_GRID_WIDTH / 2, RACK_GRID_WIDTH * 3, box.size.x - RACK_GRID_WIDTH, box.size.y - RACK_GRID_WIDTH * 6);
		nvgStroke(args.vg);
		nvgFill(args.vg);
	}

	{
		nvgStrokeColor(args.vg, getBackgroundStrokeColor());
		nvgFillColor(args.vg, getBackgroundFillColor());

		nvgStrokeWidth(args.vg, 1);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2, 5);
		nvgStroke(args.vg);
		nvgFill(args.vg);
	}

	{
		nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
		nvgFillColor(args.vg, nvgRGB(189, 189, 189));
		nvgStrokeWidth(args.vg, 1);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, box.size.x - 5 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4 + RACK_GRID_WIDTH * 0.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 17 - RACK_GRID_WIDTH * 0.25f, 5);
		nvgStroke(args.vg);
		nvgFill(args.vg);
	}

	{
		nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
		nvgFillColor(args.vg, nvgRGB(115, 115, 115));
		nvgStrokeWidth(args.vg, 1);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, box.size.x - 3 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4 + RACK_GRID_WIDTH * 0.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 17 - RACK_GRID_WIDTH * 0.25f, 5);
		nvgStroke(args.vg);
		nvgFill(args.vg);
	}

	{
		nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
		nvgFillColor(args.vg, nvgRGBA(0xc9, 0xc9, 0xc9, 0));
		nvgStrokeWidth(args.vg, 0.5f);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, RACK_GRID_WIDTH, RACK_GRID_WIDTH * 7, box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4, 5);
		nvgRoundedRect(args.vg, RACK_GRID_WIDTH, RACK_GRID_WIDTH * 11, box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 6, 5);
		nvgStroke(args.vg);
		nvgFill(args.vg);
	}

	ModuleWidget::draw(args);
}

void BaseModuleWidget::addName(const char* name, const char* digitalAtavism/* = "Digital Atavsim"*/)
{
	const int xPos = box.size.x / 2 + 1;

	{
		const int yPos = box.pos.y + RACK_GRID_WIDTH * 2;
		addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos + 2, yPos + 2, name, fontSize, nvgRGB(10, 10, 10), font));
		addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos - 2, yPos - 2, name, fontSize, nvgRGB(54, 54, 54), font));
		addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos, yPos, name, fontSize, nvgRGB(255, 255, 255), font));
	}

	{
		const int yPos = box.size.y - RACK_GRID_WIDTH * 2;
		addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos + 1, yPos + 1, digitalAtavism, fontSize, nvgRGB(10, 10, 10), font));
		addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos, yPos, digitalAtavism, fontSize, nvgRGB(255, 255, 255), font));
	}
}

} // namespace gui
} // namespace DigitalAtavism
