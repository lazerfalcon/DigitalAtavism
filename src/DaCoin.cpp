#include "DA.hpp"

namespace DigitalAtavism {

struct Coin : Module
{
	enum ParamIds
	{
		FREQ_PARAM,
		FREQ_MOD_PARAM,
		FREQ_MOD_TIME_PARAM,
		PUNCH_PARAM,
		HOLD_TIME_PARAM,
		RELEASE_TIME_PARAM,
		NUM_PARAMS
	};
	enum InputIds
	{
		PITCH_INPUT,
		PITCH_MOD_INPUT,
		FREQ_MOD_TIME_INPUT,
		HOLD_TIME_INPUT,
		RELEASE_TIME_INPUT,
		TRIGGER_PLAY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		PITCH_OUTPUT, // unused
		FREQ_MOD_TRIGGER_OUTPUT,
		VOLUME_ENV_OUTPUT,
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		NUM_LIGHTS
	};

	Coin()
	{
		INFO("DigitalAtavism - Coin: %i params  %i inputs  %i outputs  %i lights", NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, -54, 54, 0, "Frequency", " Hz", rack::dsp::FREQ_SEMITONE, rack::dsp::FREQ_C4);
		configParam(FREQ_MOD_PARAM, -24, 24, 0, "Frequency Mod", " semitones");
		configParam(FREQ_MOD_TIME_PARAM, 0, 0.40, 0.15, "Mod Time", "ms", 0, 1000);
		configParam(HOLD_TIME_PARAM, 0.001, 0.25, 0.05, "Hold Time", "ms", 0, 1000);
		configParam(RELEASE_TIME_PARAM, 0, 0.25, 0.01, "Release Time", "ms", 0, 1000);
		configParam(PUNCH_PARAM, 0.3f, 0.6f, 1, "Punch", "%", 0, 100);

		configInput(PITCH_INPUT, "1V/octave pitch");
		configInput(PITCH_MOD_INPUT, "1V/octave pitch change");
		configInput(FREQ_MOD_TIME_INPUT, "Pitch change time");
		configInput(HOLD_TIME_INPUT, "Hold time");
		configInput(RELEASE_TIME_INPUT, "Release time");
		configInput(TRIGGER_PLAY_INPUT, "Trigger");

		configOutput(FREQ_MOD_TRIGGER_OUTPUT, "Pitch change trigger");
		configOutput(VOLUME_ENV_OUTPUT, "Envelope");
		configOutput(MAIN_OUTPUT, "Output");

		onReset();
	}

	struct Osc
	{
		enum { supersampling = 8, }; // could go down to 1, if required...

		int phase{};
		int period{ 1 };

		void resetPhase()
		{
			phase = 0;
		}

		void setOscPeriod(float sampleRate, float oscPeriod)
		{
			period = clamp(int(oscPeriod * sampleRate), supersampling, 12500 * supersampling);
		}

		float process()
		{
			int ret{};
			for (int i = 0; i < supersampling; ++i)
			{
				++phase;
				phase %= period;
				ret += (phase * 2 / period == 0) * 2 - 1;
			}

			return (float)ret / 2 / supersampling;
		}
	};

	rack::dsp::SchmittTrigger triggerPlay;
	dsp::TimedTrigger freqModTrigger;
	rack::dsp::PulseGenerator freqModTimePulseGenerator;

	Osc osc;
	float oscPeriod{ 1.0f };
	dsp::HrEnvelope envelope;

	void onReset() override
	{
		onSampleRateChange();

		triggerPlay.reset();
		freqModTrigger.reset();
		freqModTimePulseGenerator.reset();

		envelope.stop();
		onRandomize();
	}

	void onRandomize() override
	{
		const auto timeRatio = 1.0f / 44100.0f;

		const auto period = 100.0f / (da::math::sqr(0.4f + da::math::frnd() * 0.5f) + 0.001f) * timeRatio;
		params[FREQ_PARAM].setValue(log2f(osc.supersampling / rack::dsp::FREQ_C4 / period) * 12.0f);

		const auto holdTime = da::math::sqr(da::math::frnd() * 0.1f) * 100000.0f * timeRatio;
		params[HOLD_TIME_PARAM].setValue(holdTime);

		const auto releaseTime = da::math::sqr(0.1f + da::math::frnd() * 0.4f) * 100000.0f * timeRatio;
		params[RELEASE_TIME_PARAM].setValue(releaseTime);

		const auto punchAmount = 0.3f + da::math::frnd() * 0.3f;
		params[PUNCH_PARAM].setValue(punchAmount);

		auto freqChangeTime = 0.0f;
		if (da::math::rnd<1>() == 1)
			freqChangeTime = (da::math::sqr(0.5f - da::math::frnd() * 0.2f) * 20000 + 32) * timeRatio;

		params[FREQ_MOD_TIME_PARAM].setValue(freqChangeTime);

		const auto sampleRate = APP->engine->getSampleRate();
		const auto freqModAmount = 1.0f - da::math::sqr(0.2f + da::math::frnd() * 0.4f) * 0.9f;
		params[FREQ_MOD_PARAM].setValue(log2f(freqModAmount) * -12.0f * 44100.0f / sampleRate);
	}

	void process(const ProcessArgs &args) override
	{
		const auto playTriggerInputConnected = inputs[TRIGGER_PLAY_INPUT].isConnected();
		const auto playTriggered = triggerPlay.process(playTriggerInputConnected ? rescale(inputs[TRIGGER_PLAY_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f) : 0.f);

		if (playTriggered || !playTriggerInputConnected)
		{
			oscPeriod = osc.supersampling / rack::dsp::FREQ_C4 * exp2f(-params[FREQ_PARAM].getValue() / 12.0f - inputs[PITCH_INPUT].getVoltage());
			osc.setOscPeriod(args.sampleRate, oscPeriod);
		}

		if (!playTriggerInputConnected)
		{
			freqModTrigger.reset();
		}
		else if (playTriggered)
		{
			auto freqChangeTime = params[FREQ_MOD_TIME_PARAM].getValue();
			if (inputs[FREQ_MOD_TIME_INPUT].isConnected())
				freqChangeTime = std::min(std::max(freqChangeTime, 0.00001f) * exp2f(inputs[FREQ_MOD_TIME_INPUT].getVoltage()), 0.8f);

			freqModTrigger.trigger(freqChangeTime);

			osc.resetPhase();

			auto holdTime = params[HOLD_TIME_PARAM].getValue();
			if (inputs[HOLD_TIME_INPUT].isConnected())
				holdTime = clamp(holdTime * exp2f(inputs[HOLD_TIME_INPUT].getVoltage()), 0.0005f, 0.5f);

			auto releaseTime = params[RELEASE_TIME_PARAM].getValue();
			if (inputs[RELEASE_TIME_INPUT].isConnected())
				releaseTime = std::min(std::max(releaseTime, 0.00001f) * exp2f(inputs[RELEASE_TIME_INPUT].getVoltage()), 0.5f);

			const auto punchAmount = params[PUNCH_PARAM].getValue();
			envelope.start(args.sampleRate, holdTime, releaseTime, punchAmount);
		}

		if (freqModTrigger.process(args.sampleTime))
		{
			const auto freqModAmount = exp2f(-params[FREQ_MOD_PARAM].getValue() / 12 - inputs[PITCH_MOD_INPUT].getVoltage());
			osc.setOscPeriod(args.sampleRate, oscPeriod * freqModAmount);
			freqModTimePulseGenerator.trigger(1e-3f);
		}

		const auto sample = osc.process();
		const auto env = envelope.process();
		const auto out = !playTriggerInputConnected ? sample : sample * env;
		outputs[MAIN_OUTPUT].setVoltage(quantize<8>(out) * 5.0f);
		outputs[VOLUME_ENV_OUTPUT].setVoltage(quantize<8>(env) * 10.0f);
		outputs[FREQ_MOD_TRIGGER_OUTPUT].setVoltage(freqModTimePulseGenerator.process(args.sampleTime) * 10.0f);
	}

	struct Widget : gui::BaseModuleWidget
	{
		using MyModule = Coin;

		Widget(MyModule* module)
		{
			setModule(module);

			box.size = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_HEIGHT);

			addName("coin");

			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
			addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 2, RACK_GRID_WIDTH * 3)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 3 * RACK_GRID_WIDTH / 2, RACK_GRID_WIDTH * 3)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 3 * RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));

			auto addTextLabelWithFx = [this](const char* text, float yPos, const char* font)
			{
				addChild(new gui::TextLabel<NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE>(RACK_GRID_WIDTH * 5 / 4 + RACK_GRID_WIDTH * 3 / 4 + 1 + RACK_GRID_WIDTH * 3 / 4 + 1, yPos + 2, text, 24, nvgRGB(189, 189, 189), font));
				addChild(new gui::TextLabel<NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE>(RACK_GRID_WIDTH * 5 / 4 + RACK_GRID_WIDTH * 3 / 4 + 1, yPos + 1, text, 24, nvgRGB(162, 162, 162), font));
				addChild(new gui::TextLabel<NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE>(RACK_GRID_WIDTH * 5 / 4, yPos, text, 24, nvgRGB(54, 54, 54), font));
			};

			const auto font = "res/fonts/HanaleiFill-Regular.ttf";
			const auto xPosKnob = box.size.x - 6 * RACK_GRID_WIDTH;
			const auto xPosInput = box.size.x - 4 * RACK_GRID_WIDTH;
			const auto xPosOutput = box.size.x - 2 * RACK_GRID_WIDTH;

			auto yPos = RACK_GRID_WIDTH * 6;
			addTextLabelWithFx("FREQ", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::FREQ_PARAM));
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosInput, yPos - RACK_GRID_WIDTH * 5 / 4, "CV", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::PITCH_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("FMOD", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::FREQ_MOD_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::PITCH_MOD_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("TIME", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::FREQ_MOD_TIME_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::FREQ_MOD_TIME_INPUT));

			{
				yPos -= RACK_GRID_WIDTH;
				addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosOutput, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontLight, smallFont));
				addOutput(createOutputCentered<PJ301MPort>(Vec(xPosOutput, yPos), module, MyModule::FREQ_MOD_TRIGGER_OUTPUT));
				yPos += RACK_GRID_WIDTH;
			}

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("HOLD", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::HOLD_TIME_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::HOLD_TIME_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("REL", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::RELEASE_TIME_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::RELEASE_TIME_INPUT));

			{
				yPos -= RACK_GRID_WIDTH;
				addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosOutput, yPos - RACK_GRID_WIDTH * 5 / 4, "ENV", smallFontSize, smallFontLight, smallFont));
				addOutput(createOutputCentered<PJ301MPort>(Vec(xPosOutput, yPos), module, MyModule::VOLUME_ENV_OUTPUT));
				yPos += RACK_GRID_WIDTH;
			}

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("PUNCH", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::PUNCH_PARAM));

			yPos += RACK_GRID_WIDTH * 4;
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosInput, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::TRIGGER_PLAY_INPUT));
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosOutput, yPos - RACK_GRID_WIDTH * 5 / 4, "OUT", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(xPosOutput, yPos), module, MyModule::MAIN_OUTPUT));
		}

		NVGcolor getBackgroundFillColor() override { return nvgRGBA(0xc6, 0xc6, 0xb4, 0x1a); }
		NVGcolor getBackgroundStrokeColor() override { return nvgRGBA(0xc6, 0xc6, 0x7b, 0x7f); }

		void draw(const DrawArgs& args) override
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
				nvgRoundedRect(args.vg, box.size.x - 5 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4 + RACK_GRID_WIDTH * 0.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 11 - RACK_GRID_WIDTH * 0.25f, 5);
				nvgRoundedRect(args.vg, box.size.x - 5 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 18 + RACK_GRID_WIDTH * 0.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3 - RACK_GRID_WIDTH * 0.25f, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			{
				nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
				nvgFillColor(args.vg, nvgRGB(115, 115, 115));
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, box.size.x - 3 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 7, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 14, 5);
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
	};
};

} // namespace DigitalAtavism

Model* modelCoin = DigitalAtavism::createDaModel<DigitalAtavism::Coin>("Coin");
