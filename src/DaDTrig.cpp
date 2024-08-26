#include "plugin.hpp"
#include "DA.hpp"

namespace DigitalAtavism {

struct DTrig : Module
{
	enum ParamIds
	{
		DELAY_TIME_PARAM,
		GATE_LENGTH_PARAM,
		NUM_PARAMS
	};
	enum InputIds
	{
		DELAY_TIME_INPUT,
		TRIGGER_PLAY_INPUT,
		GATE_LENGTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		TRIGGER_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		NUM_LIGHTS
	};

	DTrig()
	{
		INFO("DigitalAtavism - D-Trig: %i params  %i inputs  %i outputs  %i lights", NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DELAY_TIME_PARAM, 1.5e-3f, 2, 0.15f, "Delay time", " ms", 0, 1000);
		configParam(GATE_LENGTH_PARAM, 1e-3f, 1, 1e-3f, "Gate time", " ms", 0, 1000);

		configInput(DELAY_TIME_INPUT, "Delay time");
		configInput(GATE_LENGTH_INPUT, "Gate length");
		configInput(TRIGGER_PLAY_INPUT, "Trigger");

		configOutput(TRIGGER_OUTPUT, "Trigger");

		onReset();
	}

	rack::dsp::SchmittTrigger triggerPlay;
	dsp::TimedTrigger outputTrigger;
	rack::dsp::PulseGenerator pulseGenerator;
	
	void onReset() override
	{
		triggerPlay.reset();
		outputTrigger.reset();
		pulseGenerator.reset();
	}

	void process(const ProcessArgs &args) override
	{
		if (triggerPlay.process(rescale(inputs[TRIGGER_PLAY_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f)) || (!outputTrigger.isActive() && !inputs[TRIGGER_PLAY_INPUT].isConnected()))
		{
			auto delayTime = params[DELAY_TIME_PARAM].getValue();
			if (inputs[DELAY_TIME_INPUT].isConnected())
				delayTime = std::min(delayTime * exp2f(inputs[DELAY_TIME_INPUT].getVoltage()), 5.f);

			outputTrigger.trigger(delayTime);
		}

		if (outputTrigger.process(args.sampleTime))
		{
			auto gateDuration = params[GATE_LENGTH_PARAM].getValue();
			if (inputs[GATE_LENGTH_INPUT].isConnected())
				gateDuration = clamp(gateDuration * exp2f(inputs[GATE_LENGTH_INPUT].getVoltage()), 1e-3f, 5.f);

			pulseGenerator.trigger(gateDuration);
		}

		outputs[TRIGGER_OUTPUT].setVoltage(pulseGenerator.process(args.sampleTime) * 10.f);
	}

	struct Widget : gui::BaseModuleWidget
	{
		using MyModule = DTrig;

		Widget(MyModule* module)
		{
			setModule(module);

			box.size = Vec(RACK_GRID_WIDTH * 3, RACK_GRID_HEIGHT);

			addName("d-trig", "DA");

			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 5 * RACK_GRID_WIDTH / 4, RACK_GRID_WIDTH * 3)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 4, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 4, RACK_GRID_WIDTH * 3)));

			auto yPos = RACK_GRID_WIDTH * 6;
			const auto xPos = box.size.x / 2;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos, yPos - RACK_GRID_WIDTH * 5 / 4, "DLAY", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, DELAY_TIME_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPos, yPos), module, DELAY_TIME_PARAM));

			yPos += RACK_GRID_WIDTH * 3.25f;
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos, yPos - RACK_GRID_WIDTH * 5 / 4, "TIME", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, GATE_LENGTH_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPos, yPos), module, GATE_LENGTH_PARAM));

			yPos += RACK_GRID_WIDTH * 3.25f;
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPos, yPos), module, TRIGGER_PLAY_INPUT));

			yPos += RACK_GRID_WIDTH * 3.25f;
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPos, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(xPos, yPos), module, TRIGGER_OUTPUT));
		}

		void draw(const DrawArgs& args) override
		{
			{
				nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
				nvgFillColor(args.vg, nvgRGB(0xc9, 0xc9, 0xc9));
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, RACK_GRID_WIDTH / 4, RACK_GRID_WIDTH * 3, box.size.x - RACK_GRID_WIDTH / 2, box.size.y - RACK_GRID_WIDTH * 6);
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
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 4.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 5, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 9.5f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 5, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 14.75f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			{
				nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
				nvgFillColor(args.vg, nvgRGB(115, 115, 115));
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 18.f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			ModuleWidget::draw(args);
		}

		NVGcolor getBackgroundFillColor() override { return nvgRGBA(0xb4, 0xb4, 0xc6, 0x1a); }
		NVGcolor getBackgroundStrokeColor() override { return nvgRGBA(0x7b, 0xc6, 0xc6, 0x7f); }
	};
};

} // namespace DigitalAtavism

Model* modelDTrig = DigitalAtavism::createDaModel<DigitalAtavism::DTrig>("DTrig");
