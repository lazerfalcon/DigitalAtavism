#include "DA.hpp"

namespace DigitalAtavism {

struct Hit : Module
{
	enum ParamIds
	{
		FREQ_PARAM,
		FREQ_SLIDE_PARAM,

		TYPE_SELECTION_PARAM,
		SQUARE_DUTY_PARAM,

		HOLD_TIME_PARAM,
		RELEASE_TIME_PARAM,

		HIPASS_FILTER_PARAM,
		LOPASS_FILTER_PARAM,

		NUM_PARAMS
	};
	enum InputIds
	{
		PITCH_INPUT,
		FREQ_SLIDE_INPUT,

		BLEND_INPUT,
		SQUARE_DUTY_INPUT,

		HOLD_TIME_INPUT,
		RELEASE_TIME_INPUT,

		TRIGGER_PLAY_INPUT,

		NUM_INPUTS
	};
	enum OutputIds
	{
		VOLUME_ENV_OUTPUT,
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		NUM_LIGHTS
	};

	Hit()
	{
		INFO("DigitalAtavism - Hit: %i params  %i inputs  %i outputs  %i lights", NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, -54, 54, 0, "Frequency", " Hz", rack::dsp::FREQ_SEMITONE, rack::dsp::FREQ_C4);
		configParam(FREQ_SLIDE_PARAM, -1, 1, 0, "Frequency Slide", "%", 0, 100);
		configParam(TYPE_SELECTION_PARAM, 0, 3, 0, "Blend", "%", 0, 100.0f / 3);
		configParam(SQUARE_DUTY_PARAM, 0.01, 0.99, 0.5, "Pulse Width", "%", 0, 100);
		configParam(HOLD_TIME_PARAM, 0.001, 0.25, 0.05, "Hold Time", "ms", 0, 1000);
		configParam(RELEASE_TIME_PARAM, 0, 0.25, 0.01, "Release Time", "ms", 0, 1000);
		configParam(HIPASS_FILTER_PARAM, 0, 1, 0, "HPF", "%", 0, 100);
		configParam(LOPASS_FILTER_PARAM, 0, 1, 0, "LPF", "%", 0, 100);

		configInput(PITCH_INPUT, "1V/octave pitch");
		configInput(FREQ_SLIDE_INPUT, "Slide");
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

		float fperiod{ 1.f };
		float fslide{ 1.f };

		enum { noise_buffer_size = 32 };
		int noise_buffer_index{ -1 };
		float noise_value{};

		float fltp{};
		float fltdp{};
		float fltw{ 0.1f };
		float flthp{};
		float fltphp{};

		void resetPhase()
		{
			phase = 0;
		}

		void resetFilter()
		{
			fltp = fltdp = fltphp = 0.f;
		}

		float getNoise() const
		{
			return da::math::frnd() - 0.5f;
		}

		void setOscPeriod(float sampleRate, float oscPeriod)
		{
			fperiod = oscPeriod * sampleRate;
		}

		void setLoPassFilter(float value)
		{
			value *= 0.9f;
			value += 0.1f;
			fltw = value * value * value * 0.1f;
		}

		void setHiPassFilter(float value)
		{
			flthp = da::math::sqr(value) * 0.1f;
		}

		void setFreqSlide(float value)
		{
			fslide = 1.f + value * value * value * 0.01f;
		}

		float process(float wave_type, float square_duty)
		{
			float ret{};

			fperiod *= fslide;

			int period = (int)fperiod;
			if (period < supersampling)
			{
				fperiod = (period = supersampling);
				fslide = 1.0f;
			}
			else if (period > 50000 * supersampling)
			{
				fperiod = (period = 50000 * supersampling);
				fslide = 1.0f;
			}

			const int duty = (int)(square_duty * period);

			for (int i = 0; i < supersampling; ++i)
			{
				++phase;
				phase %= period;

				// base waveform
				const float fp = (float)phase / period;
				const auto square = phase <= duty ? 0.5f : -0.5f;
				const auto sawtooth = 1.0f - fp * 2;

				const auto previous_noise_buffer_index = noise_buffer_index;
				noise_buffer_index = phase * noise_buffer_size / period;
				if (previous_noise_buffer_index != noise_buffer_index)
					noise_value = getNoise();

				const auto noise = noise_value;
				const auto sample = wave_type <= 1.0f ? 
					(1.0f - wave_type) * square + sawtooth * wave_type : 
					wave_type <= 2.0f ? (2.0f - wave_type) * sawtooth + noise * (wave_type - 1.0f) :
					(3.0f - wave_type) * noise + square * (wave_type - 2.0f);

				// lp filter
				const auto pp = fltp;
				fltdp += (sample - fltp) * fltw;
				const float fltdmp = 5.0f / (1.0f + da::math::sqr(0.4f * fltw) * 20.0f) * (0.04f + fltw);
				//if (fltdmp > 0.8f) fltdmp = 0.8f;
				fltdp -= fltdp * fltdmp;
				fltp += fltdp;

				// hp filter
				fltphp += fltp - pp;
				fltphp -= fltphp * flthp;

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

		const auto period = 100.0f / (da::math::sqr(0.2f + da::math::frnd() * 0.6f) + 0.001f) * timeRatio;
		params[FREQ_PARAM].setValue(log2f(osc.supersampling / rack::dsp::FREQ_C4 / period) * 12.0f);

		const auto waveType = da::math::rnd<2>();
		params[TYPE_SELECTION_PARAM].setValue(waveType);

		const auto squareDuty = da::math::frnd() * 0.6f;
		params[SQUARE_DUTY_PARAM].setValue(squareDuty);

		const auto holdTime = da::math::sqr(da::math::frnd() * 0.1f) * 100000.0f * timeRatio;
		params[HOLD_TIME_PARAM].setValue(holdTime);

		const auto releaseTime = da::math::sqr(0.1f + da::math::frnd() * 0.2f) * 100000.0f * timeRatio;
		params[RELEASE_TIME_PARAM].setValue(releaseTime);

		params[FREQ_SLIDE_PARAM].setValue(0.3f + da::math::frnd() * 0.4f);

		params[HIPASS_FILTER_PARAM].setValue(da::math::rnd<1>() ? da::math::frnd() * 0.3f : 0.0f);

		params[LOPASS_FILTER_PARAM].setValue(0.6f + da::math::frnd() * 0.4f);
	}

	void process(const ProcessArgs &args) override
	{
		const auto playTriggerInputConnected = inputs[TRIGGER_PLAY_INPUT].isConnected();
		const auto playTriggered = triggerPlay.process(playTriggerInputConnected ? rescale(inputs[TRIGGER_PLAY_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f) : 0.f);

		if (playTriggered || !playTriggerInputConnected)
		{
			const auto oscPeriod = osc.supersampling / rack::dsp::FREQ_C4 * exp2f(-params[FREQ_PARAM].getValue() / 12.0f - inputs[PITCH_INPUT].getVoltage());
			osc.setOscPeriod(args.sampleRate, oscPeriod);
			osc.setLoPassFilter(params[LOPASS_FILTER_PARAM].getValue());
			osc.setHiPassFilter(params[HIPASS_FILTER_PARAM].getValue());
		}

		if (playTriggered)
		{
			osc.resetPhase();
			osc.resetFilter();
			osc.setFreqSlide(clamp(-params[FREQ_SLIDE_PARAM].getValue() - inputs[FREQ_SLIDE_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

			auto holdTime = params[HOLD_TIME_PARAM].getValue();
			if (inputs[HOLD_TIME_INPUT].isConnected())
				holdTime = clamp(holdTime * exp2f(inputs[HOLD_TIME_INPUT].getVoltage()), 0.0005f, 0.5f);

			auto releaseTime = params[RELEASE_TIME_PARAM].getValue();
			if (inputs[RELEASE_TIME_INPUT].isConnected())
				releaseTime = std::min(std::max(releaseTime, 0.00001f) * exp2f(inputs[RELEASE_TIME_INPUT].getVoltage()), 0.5f);

			envelope.start(args.sampleRate, holdTime, releaseTime);
		}

		const float wave_type = clamp(params[TYPE_SELECTION_PARAM].getValue() + inputs[BLEND_INPUT].getVoltage() * 0.1f * 3.f, 0.f, 3.f);
		const float square_duty = clamp(params[SQUARE_DUTY_PARAM].getValue() + inputs[SQUARE_DUTY_INPUT].getVoltage() * 0.1f, 0.01f, 0.99f);
		const auto sample = osc.process(wave_type, square_duty);
		const auto env = envelope.process();
		const auto out = !playTriggerInputConnected ? sample : sample * env;
		outputs[MAIN_OUTPUT].setVoltage(quantize<8>(out) * 5.0f);
		outputs[VOLUME_ENV_OUTPUT].setVoltage(quantize<8>(env) * 10.0f);
	}

	struct Widget : gui::BaseModuleWidget
	{
		using MyModule = Hit;

		Widget(MyModule* module)
		{
			setModule(module);

			box.size = Vec(RACK_GRID_WIDTH * 12, RACK_GRID_HEIGHT);

			addName("hit");

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
			addTextLabelWithFx("SLIDE", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::FREQ_SLIDE_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::FREQ_SLIDE_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("BLND", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::TYPE_SELECTION_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::BLEND_INPUT));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("PW", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::SQUARE_DUTY_PARAM));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::SQUARE_DUTY_INPUT));

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
			addTextLabelWithFx("LPF", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::LOPASS_FILTER_PARAM));

			yPos += RACK_GRID_WIDTH * 2;
			addTextLabelWithFx("HPF", yPos, font);
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(xPosKnob, yPos), module, MyModule::HIPASS_FILTER_PARAM));
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosInput, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(xPosInput, yPos), module, MyModule::TRIGGER_PLAY_INPUT));
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(xPosOutput, yPos - RACK_GRID_WIDTH * 5 / 4, "OUT", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(xPosOutput, yPos), module, MyModule::MAIN_OUTPUT));
		}

		NVGcolor getBackgroundFillColor() override { return nvgRGBA(0xc6, 0xc6, 0xb4, 0x1a); }
		NVGcolor getBackgroundStrokeColor() override { return nvgRGBA(0xc6, 0x7b, 0x7b, 0x7f); }

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
				nvgRoundedRect(args.vg, box.size.x - 5 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4 + RACK_GRID_WIDTH * 0.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 13 - RACK_GRID_WIDTH * 0.25f, 5);
				nvgRoundedRect(args.vg, box.size.x - 5 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 18 + RACK_GRID_WIDTH * 0.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3 - RACK_GRID_WIDTH * 0.25f, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			{
				nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
				nvgFillColor(args.vg, nvgRGB(115, 115, 115));
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, box.size.x - 3 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 13, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 8, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			{
				nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
				nvgFillColor(args.vg, nvgRGBA(0xc9, 0xc9, 0xc9, 0));
				nvgStrokeWidth(args.vg, 0.5f);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH, RACK_GRID_WIDTH * 9, box.size.x - 4 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH, RACK_GRID_WIDTH * 13, box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_WIDTH * 4, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			ModuleWidget::draw(args);
		}
	};
};

} // namespace DigitalAtavism

Model* modelHit = DigitalAtavism::createDaModel<DigitalAtavism::Hit>("Hit");
