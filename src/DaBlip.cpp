#include "DA.hpp"

namespace DigitalAtavism {

struct Blip : Module
{
	enum ParamIds
	{
		FREQ_PARAM,
		TYPE_SELECTION_PARAM,
		SQUARE_DUTY_PARAM,
		MASTER_VOLUME_PARAM,
		HOLD_TIME_PARAM,
		RELEASE_TIME_PARAM,
		NUM_PARAMS
	};
	enum InputIds
	{
		PITCH_INPUT,
		BLEND_INPUT,
		SQUARE_DUTY_INPUT,
		HOLD_TIME_INPUT,
		RELEASE_TIME_INPUT,
		TRIGGER_PLAY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		PITCH_OUTPUT, // unused
		OSC_OUTPUT, // unused
		VOLUME_ENV_OUTPUT,
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		NUM_LIGHTS
	};

	Blip()
	{
		INFO("DigitalAtavism - Blip: %i params  %i inputs  %i outputs  %i lights", NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, -54, 54, 0, "Frequency", " Hz", rack::dsp::FREQ_SEMITONE, rack::dsp::FREQ_C4);
		configParam(TYPE_SELECTION_PARAM, 0, 1, 0, "Blend", "%", 0, 100);
		configParam(SQUARE_DUTY_PARAM, 0.01, 0.99, 0.5, "Pulse Width", "%", 0, 100);
		configParam(HOLD_TIME_PARAM, 0.001, 0.25, 0.05, "Hold Time", "ms", 0, 1000);
		configParam(RELEASE_TIME_PARAM, 0, 0.25, 0.01, "Release Time", "ms", 0, 1000);

		configInput(PITCH_INPUT, "1V/octave pitch");
		configInput(BLEND_INPUT, "Blend");
		configInput(SQUARE_DUTY_INPUT, "Pulse width modulation");
		configInput(HOLD_TIME_INPUT, "Hold time");
		configInput(RELEASE_TIME_INPUT, "Release time");
		configInput(TRIGGER_PLAY_INPUT, "Trigger");

		configOutput(VOLUME_ENV_OUTPUT, "Envelope");
		configOutput(MAIN_OUTPUT, "Output");

		onReset();
	}

	struct Osc
	{
		enum { supersampling = 8, }; // could go down to 1, if required...

		int phase{};
		int period{ 1 };

		float fltp{};
		float fltphp{};

		void resetPhase()
		{
			phase = 0;
		}

		void resetFilter()
		{
			fltp = 0.0f;
			fltphp = 0.0f;
		}

		void setOscPeriod(float sampleRate, float oscPeriod)
		{
			period = clamp(int(oscPeriod * sampleRate), supersampling, 12500 * supersampling);
		}

		float process(float wave_type, float square_duty)
		{
			float ret{};
			const int duty = (int)(square_duty * period);
			for (int i = 0; i < supersampling; ++i)
			{
				++phase;
				phase %= period;

				// base waveform
				const float fp = (float)phase / period;
				const auto square = phase <= duty ? 0.5f : -0.5f;
				const auto sawtooth = 1.0f - fp * 2;
				const auto sample = (1.0f - wave_type) * square + sawtooth * wave_type;

				// hp filter
				const auto pp = fltp;
				fltp = sample;
				fltphp += fltp - pp;
				fltphp *= 0.999f;

				ret += fltphp;
			}

			return ret / supersampling;
		}
	};

	rack::dsp::SchmittTrigger triggerPlay;

	Osc osc;
	dsp::HrEnvelope envelope;

	void onReset() override
	{
		onSampleRateChange();

		triggerPlay.reset();

		envelope.stop();
		onRandomize();
	}

	void onRandomize() override
	{
		const auto timeRatio = 1.0f / 44100.0f;

		const auto period = 100.0 / (da::math::sqr(0.2f + da::math::frnd() * 0.4f) + 0.001) * timeRatio;
		params[FREQ_PARAM].setValue(log2f(osc.supersampling / rack::dsp::FREQ_C4 / period) * 12.0f);

		const auto waveType = da::math::rnd<1>();
		params[TYPE_SELECTION_PARAM].setValue(waveType);

		const auto holdTime = da::math::sqr(0.1f + da::math::frnd() * 0.1f) * 100000.0f * timeRatio;
		params[HOLD_TIME_PARAM].setValue(holdTime);

		const auto releaseTime = da::math::sqr(da::math::frnd() * 0.2f) * 100000.0f * timeRatio;
		params[RELEASE_TIME_PARAM].setValue(releaseTime);

		const auto squareDuty = 0.5f - da::math::frnd() * 0.3f;
		params[SQUARE_DUTY_PARAM].setValue(squareDuty);
	}

	void process(const ProcessArgs &args) override
	{
		const auto playTriggerInputConnected = inputs[TRIGGER_PLAY_INPUT].isConnected();
		const auto playTriggered = triggerPlay.process(playTriggerInputConnected ? rescale(inputs[TRIGGER_PLAY_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f) : 0.f);

		if (playTriggered || !playTriggerInputConnected)
		{
			const auto oscPeriod = osc.supersampling / rack::dsp::FREQ_C4 * exp2f(-params[FREQ_PARAM].getValue() / 12.0f - inputs[PITCH_INPUT].getVoltage());
			osc.setOscPeriod(args.sampleRate, oscPeriod);
		}

		if (playTriggered)
		{
			osc.resetPhase();
			osc.resetFilter();

			auto holdTime = params[HOLD_TIME_PARAM].getValue();
			if (inputs[HOLD_TIME_INPUT].isConnected())
				holdTime = clamp(holdTime * exp2f(inputs[HOLD_TIME_INPUT].getVoltage()), 0.0005f, 0.5f);

			auto releaseTime = params[RELEASE_TIME_PARAM].getValue();
			if (inputs[RELEASE_TIME_INPUT].isConnected())
				releaseTime = std::min(std::max(releaseTime, 0.00001f) * exp2f(inputs[RELEASE_TIME_INPUT].getVoltage()), 0.5f);

			envelope.start(args.sampleRate, holdTime, releaseTime, 0.0f);
		}

		const float wave_type = clamp(params[TYPE_SELECTION_PARAM].getValue() + inputs[BLEND_INPUT].getVoltage() * 0.1f, 0.0f, 1.0f);
		const float square_duty = clamp(params[SQUARE_DUTY_PARAM].getValue() + inputs[SQUARE_DUTY_INPUT].getVoltage() * 0.1f, 0.01f, 0.99f);
		const auto sample = osc.process(wave_type, square_duty);
		const auto env = envelope.process();
		const auto out = !playTriggerInputConnected ? sample : sample * env;
		outputs[MAIN_OUTPUT].setVoltage(quantize<8>(out) * 5.0f);
		outputs[VOLUME_ENV_OUTPUT].setVoltage(quantize<8>(env) * 10.0f);
	}

	struct Widget : gui::BaseModuleWidget
	{
		using MyModule = Blip;

		Widget(MyModule* module)
		{
			setModule(module);

			box.size = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_HEIGHT);

			addName("blip");

			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
			addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 2, RACK_GRID_WIDTH * 3)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 3 * RACK_GRID_WIDTH / 2, RACK_GRID_WIDTH * 3)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 3 * RACK_GRID_WIDTH / 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));

			auto addTextLabelWithFx = [this](const char* text, float yPos)
			{
				const auto font = "res/fonts/HanaleiFill-Regular.ttf";
				addChild(new gui::TextLabel<NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE>(RACK_GRID_WIDTH * 5 / 4 + RACK_GRID_WIDTH * 3 / 4 + 1 + RACK_GRID_WIDTH * 3 / 4 + 1, yPos + 2, text, 24, nvgRGB(189, 189, 189), font));
				addChild(new gui::TextLabel<NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE>(RACK_GRID_WIDTH * 5 / 4 + RACK_GRID_WIDTH * 3 / 4 + 1, yPos + 1, text, 24, nvgRGB(162, 162, 162), font));
				addChild(new gui::TextLabel<NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE>(RACK_GRID_WIDTH * 5 / 4, yPos, text, 24, nvgRGB(54, 54, 54), font));
			};

			const auto xPosKnob = box.size.x - 6 * RACK_GRID_WIDTH;
			const auto xPosInput = box.size.x - 4 * RACK_GRID_WIDTH;
			const auto xPosOutput = box.size.x - 2 * RACK_GRID_WIDTH;

			auto yPos = RACK_GRID_WIDTH * 6;
			addTextLabelWithFx("FREQ", yPos);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::FREQ_PARAM));
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosInput, yPos - RACK_GRID_WIDTH * 5 / 4, "CV", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::PITCH_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("BLND", yPos);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::TYPE_SELECTION_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::BLEND_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("PW", yPos);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::SQUARE_DUTY_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::SQUARE_DUTY_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("HOLD", yPos);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::HOLD_TIME_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::HOLD_TIME_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("REL", yPos);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::RELEASE_TIME_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::RELEASE_TIME_INPUT));

			{
				yPos -= RACK_GRID_WIDTH;
				addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosOutput, yPos - RACK_GRID_WIDTH * 5 / 4, "ENV", smallFontSize, smallFontLight, smallFont));
				addOutput(createOutputCentered<PJ301MPort>(Vec(xPosOutput, yPos), module, MyModule::VOLUME_ENV_OUTPUT));
				yPos += RACK_GRID_WIDTH;
			}

			yPos += RACK_GRID_WIDTH * 6;
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosInput, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::TRIGGER_PLAY_INPUT));
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosOutput, yPos - RACK_GRID_WIDTH * 5 / 4, "OUT", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(xPosOutput, yPos), module, MyModule::MAIN_OUTPUT));
		}

		NVGcolor getBackgroundFillColor() override { return nvgRGBA(0xb4, 0xb4, 0xc6, 0x1a); }
		NVGcolor getBackgroundStrokeColor() override { return nvgRGBA(0x7b, 0xc6, 0xc6, 0x7f); }

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
				nvgRoundedRect(args.vg, box.size.x - 3 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 11, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 10, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			{
				nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
				nvgFillColor(args.vg, nvgRGBA(0xc9, 0xc9, 0xc9, 0));
				nvgStrokeWidth(args.vg, 0.5f);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH, RACK_GRID_WIDTH * 7, box.size.x - 4 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH, RACK_GRID_WIDTH * 11, box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			ModuleWidget::draw(args);
		}
	};
};

} // namespace DigitalAtavism

Model* modelBlip = DigitalAtavism::createDaModel<DigitalAtavism::Blip>("Blip");
