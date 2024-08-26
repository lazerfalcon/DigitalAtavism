#include "DA.hpp"

namespace DigitalAtavism {

namespace VoltageRange {

enum Value
{
	RangeA,
	RangeB,
	RangeC,
	RangeD,
	RangeE,
	RangeF,
	RangeG,

	COUNT
};

const char* GetName(Value range)
{
	switch (range)
	{
	case RangeA:
		return "0V - 10V";
	case RangeB:
		return "0V - 5V";
	case RangeC:
		return "0V - 1V";

	case RangeD:
		return "-10V - 10V";
	case RangeE:
		return "-5V - 5V";
	case RangeF:
		return "-2.5V - 2.5V";
	case RangeG:
		return "-1V - 1V";

	default:
		return nullptr;
	}
}

float GetMinimum(Value range)
{
	switch (range)
	{
	case RangeA:
	case RangeB:
	case RangeC:
	default:
		return 0.0f;

	case RangeD:
		return -10.0f;
	case RangeE:
		return -5.0f;
	case RangeF:
		return -2.5f;
	case RangeG:
		return -1.0f;
	}
}

float GetMaximum(Value range)
{
	switch (range)
	{
	case RangeA:
		return 10.0f;
	case RangeB:
		return 5.0f;
	case RangeC:
		return 1.0f;

	case RangeD:
		return 10.0f;
	case RangeE:
		return 5.0f;
	case RangeF:
		return 2.5f;
	case RangeG:
		return 1.0f;

	default:
		return 1.0f; // ensure that min and max are not the same 
	}
}

} // namespace VoltageRange

struct UT_Base : Module
{
	VoltageRange::Value voltageRange{ VoltageRange::RangeA };

	json_t* dataToJson() override
	{
		if (json_t* rootJ = json_object())
		{
			json_object_set_new(rootJ, "voltageRange", json_integer(voltageRange));
			return rootJ;
		}

		return nullptr;
	}

	void dataFromJson(json_t* rootJ) override
	{
		voltageRange = VoltageRange::RangeA;

		if (rootJ)
			if (auto voltageRangeJson = json_object_get(rootJ, "voltageRange"))
				voltageRange = static_cast<VoltageRange::Value>(json_integer_value(voltageRangeJson));
	}

	struct BaseWidget : gui::BaseModuleWidget
	{
		using BasicMenuItem = gui::BasicMenuItem<UT_Base>;

		struct VoltageRangeSubmenuItem : BasicMenuItem
		{
			Menu* createChildMenu() override
			{
				struct VoltageRangeItem : BasicMenuItem
				{
					VoltageRangeItem(VoltageRange::Value voltageRange) : voltageRange(voltageRange) {}

					VoltageRange::Value voltageRange{};
					void onAction(const event::Action& e) override
					{
						module->voltageRange = voltageRange;
					}
				};

				Menu* menu = new Menu;
				for (auto i = 0; i < VoltageRange::COUNT; ++i)
				{
					const auto voltageRange = static_cast<VoltageRange::Value>(i);
					menu->addChild(createMenuItem<VoltageRangeItem>(module, VoltageRange::GetName(voltageRange), CHECKMARK(module->voltageRange == voltageRange), voltageRange));
				}
				return menu;
			}

			static MenuItem* create(UT_Base* module)
			{
				const std::string rightText = VoltageRange::GetName(module->voltageRange);
				return createMenuItem<VoltageRangeSubmenuItem>(module, "Voltage Range:", rightText + "  " + RIGHT_ARROW);
			}
		};

		void appendContextMenu(Menu* menu) override
		{
			menu->addChild(new MenuSeparator);
			menu->addChild(VoltageRangeSubmenuItem::create(dynamic_cast<UT_Base*>(module)));
		}
	};

	struct ExpanderMessages
	{
		int numberOfInputs{ PORT_MAX_CHANNELS };
		float inputs[PORT_MAX_CHANNELS]{};
	};

	static int calculateChannelIndex(int channels, float voltage, float minimumVoltage = 0.0f, float maximumVoltage = 10.0f)
	{
		if (channels <= 1)
			return 0;

		const auto n_1 = channels - 1;
		return clamp(static_cast<int>(n_1 * ((voltage - minimumVoltage) / (maximumVoltage - minimumVoltage)) + 0.5f), 0, n_1);
	}

	static bool checkModel(const Module* module, const Model* model)
	{
		return module && module->model == model;
	}

	template<typename Models>
	static bool checkModels(const Module* module, Models models)
	{
		if (module)
			for (const auto& model : models)
				if (module->model == model)
					return true;

		return false;
	}
};



struct UT_P : UT_Base
{
	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		CV_INPUT,
		SIGNAL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		GATE_OUTPUT,
		TRIGGER_OUTPUT,
		SIGNAL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		GATE_LIGHT,
		TRIGGER_LIGHT,
		NUM_LIGHTS
	};

	UT_P()
	{
		INFO("DigitalAtavism - UT-P: %i params  %i inputs  %i outputs  %i lights", NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configInput(CV_INPUT, "Selector");
		configInput(SIGNAL_INPUT, "Signal");

		configOutput(GATE_OUTPUT, "Gate");
		configOutput(TRIGGER_OUTPUT, "Trigger");
		configOutput(SIGNAL_OUTPUT, "Selected signal");

		onReset();
	}

	rack::dsp::BooleanTrigger triggers[PORT_MAX_CHANNELS];
	rack::dsp::PulseGenerator pulseGenerators[PORT_MAX_CHANNELS];

	void onReset() override
	{
		for (auto& trigger : triggers)
			trigger.reset();

		for (auto& pulseGenerator : pulseGenerators)
			pulseGenerator.reset();
	}

	void process(const ProcessArgs &args) override
	{
		const auto minimumVoltage{ VoltageRange::GetMinimum(voltageRange) };
		const auto maximumVoltage{ VoltageRange::GetMaximum(voltageRange) };

		const auto numberOfInputChannels = inputs[SIGNAL_INPUT].getChannels();
		const auto polyCV = inputs[CV_INPUT].isPolyphonic();
		const auto numberOfOutputChannels = polyCV ? inputs[CV_INPUT].getChannels() : numberOfInputChannels ? numberOfInputChannels : 1;

		outputs[GATE_OUTPUT].setChannels(numberOfOutputChannels);
		outputs[SIGNAL_OUTPUT].setChannels(numberOfOutputChannels);

		if (!polyCV || !numberOfInputChannels)
		{
			outputs[GATE_OUTPUT].clearVoltages();
			outputs[SIGNAL_OUTPUT].clearVoltages();
		}

		outputs[TRIGGER_OUTPUT].setChannels(numberOfOutputChannels);
		outputs[TRIGGER_OUTPUT].clearVoltages();

		if (numberOfInputChannels)
		{
			if (polyCV)
			{
				for (int i = 0; i < numberOfOutputChannels; ++i)
				{
					const auto channel = calculateChannelIndex(numberOfInputChannels, inputs[CV_INPUT].getVoltage(i), minimumVoltage, maximumVoltage);
					outputs[GATE_OUTPUT].setVoltage(10.0f, i);
					outputs[SIGNAL_OUTPUT].setVoltage(inputs[SIGNAL_INPUT].getVoltage(channel), i);

					if (triggers[i].process(channel == i))
						pulseGenerators[i].trigger(1e-3f);
				}
			}
			else
			{
				const auto channel = calculateChannelIndex(numberOfInputChannels, inputs[CV_INPUT].getVoltage(), minimumVoltage, maximumVoltage);
				outputs[GATE_OUTPUT].setVoltage(10.0f, channel);
				outputs[SIGNAL_OUTPUT].setVoltage(inputs[SIGNAL_INPUT].getVoltage(channel), channel);

				for (int i = 0; i < numberOfOutputChannels; ++i)
					if (triggers[i].process(channel == i))
						pulseGenerators[i].trigger(1e-3f);
			}
		}

		for (int i = numberOfInputChannels ? numberOfOutputChannels : 0; i < PORT_MAX_CHANNELS; ++i)
		{
			triggers[i].reset();
			pulseGenerators[i].reset();
		}

		for (int i = 0; i < numberOfOutputChannels; ++i)
			outputs[TRIGGER_OUTPUT].setVoltage(pulseGenerators[i].process(args.sampleTime) ? 10.f : 0.f, i);

		if (checkModel(rightExpander.module, modelUtox))
		{
			auto& rightLeftExpander = rightExpander.module->leftExpander;
			auto& producerMessage = *reinterpret_cast<ExpanderMessages*>(rightLeftExpander.producerMessage);

			const auto channels = producerMessage.numberOfInputs = inputs[SIGNAL_INPUT].getChannels();
			for (int i = 0; i < channels; ++i)
				producerMessage.inputs[i] = inputs[SIGNAL_INPUT].getVoltage(i);

			rightLeftExpander.messageFlipRequested = true;
		}
	}

	struct Widget : BaseWidget
	{
		using MyModule = UT_P;

		widget::Widget* innerScrew{};
		widget::Widget* outterScrew{};

		Widget(MyModule* module)
		{
			setModule(module);

			box.size = Vec(RACK_GRID_WIDTH * 3, RACK_GRID_HEIGHT);

			addName("ut-p", "DA");

			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

			addChild(innerScrew = createWidget<ScrewSilver>(Vec(box.size.x - 5 * RACK_GRID_WIDTH / 4, RACK_GRID_WIDTH * 3)));
			innerScrew->show();
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 4, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));
			addChild(outterScrew = createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 4, RACK_GRID_WIDTH * 3)));
			outterScrew->hide();

			auto yPos = RACK_GRID_WIDTH * 6;
			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "IN", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, MyModule::SIGNAL_INPUT));
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "CV", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, MyModule::CV_INPUT));
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "GATE", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, MyModule::GATE_OUTPUT));
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, MyModule::TRIGGER_OUTPUT));
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "OUT", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, MyModule::SIGNAL_OUTPUT));
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
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 4.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			{
				nvgStrokeColor(args.vg, nvgRGB(54, 54, 54));
				nvgFillColor(args.vg, nvgRGB(115, 115, 115));
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 10.75f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 14.00f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 17.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgStroke(args.vg);
				nvgFill(args.vg);
			}

			ModuleWidget::draw(args);
		}

		void step() override
		{
			if (!module)
				return;

			if (checkModel(module->rightExpander.module, modelUtox))
			{
				innerScrew->hide();
				outterScrew->show();
			}
			else
			{
				innerScrew->show();
				outterScrew->hide();
			}

			gui::BaseModuleWidget::step();
		}

		NVGcolor getBackgroundFillColor() override { return nvgRGBA(0xb4, 0xb4, 0xc6, 0x1a); }
		NVGcolor getBackgroundStrokeColor() override { return nvgRGBA(0x7b, 0xc6, 0xc6, 0x7f); }
	};
};

struct UT_OX : UT_Base
{
	ExpanderMessages leftMessages[2];

	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		TRIGGER_OUTPUT,
		SIGNAL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		NUM_LIGHTS
	};

	UT_OX()
	{
		INFO("DigitalAtavism - UT-OX: %i params  %i inputs  %i outputs  %i lights", NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		leftExpander.producerMessage = &leftMessages[0];
		leftExpander.consumerMessage = &leftMessages[1];

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configInput(CV_INPUT, "Selector");

		configOutput(TRIGGER_OUTPUT, "Trigger");
		configOutput(SIGNAL_OUTPUT, "Selected signal");

		onReset();
	}

	rack::dsp::BooleanTrigger trigger;
	rack::dsp::PulseGenerator pulseGenerator;

	int selectedMonoChannel{};

	void onReset() override
	{
		trigger.reset();
		pulseGenerator.reset();
	}

	void process(const ProcessArgs &args) override
	{
		const auto models = { modelUtp, modelUtox };
		const auto isLeftExpanderValid = checkModels(leftExpander.module, models);
		const ExpanderMessages emptyMessages;
		const auto& consumerMessage = isLeftExpanderValid ? *reinterpret_cast<const ExpanderMessages*>(leftExpander.consumerMessage) : emptyMessages;
		const auto previousSelectedMonoChannel = selectedMonoChannel;
		selectedMonoChannel = calculateChannelIndex(consumerMessage.numberOfInputs, inputs[CV_INPUT].getVoltage(), VoltageRange::GetMinimum(voltageRange), VoltageRange::GetMaximum(voltageRange));
		outputs[SIGNAL_OUTPUT].setVoltage(consumerMessage.inputs[selectedMonoChannel]);

		if (trigger.process(selectedMonoChannel != previousSelectedMonoChannel))
			pulseGenerator.trigger(1e-3f);

		outputs[TRIGGER_OUTPUT].setVoltage(pulseGenerator.process(args.sampleTime) ? 10.f : 0.f);

		if (checkModel(rightExpander.module, modelUtox))
		{
			auto& expander = rightExpander.module->leftExpander;
			auto& producerMessage = *reinterpret_cast<ExpanderMessages*>(expander.producerMessage);
			producerMessage = consumerMessage;
			expander.messageFlipRequested = true;
		}
	}

	struct Widget : BaseWidget
	{
		using MyModule = UT_OX;

		Widget(MyModule* module)
		{
			setModule(module);

			box.size = Vec(RACK_GRID_WIDTH * 3, RACK_GRID_HEIGHT);

			addName("ut-ox", "DA");

			// these screws may have to be hidden when this module has a right expander
			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 5 * RACK_GRID_WIDTH / 4, RACK_GRID_WIDTH * 3)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH / 4, RACK_GRID_HEIGHT - RACK_GRID_WIDTH * 4)));

			auto yPos = RACK_GRID_WIDTH * 6;
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "CV", smallFontSize, smallFontDark, smallFont));
			addInput(createInputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, CV_INPUT));
			yPos += RACK_GRID_WIDTH * 3.25f;
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "TRIG", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, TRIGGER_OUTPUT));
			yPos += RACK_GRID_WIDTH * 3.25f;

			addChild(new gui::TextLabel<NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE>(box.size.x / 2, yPos - RACK_GRID_WIDTH * 5 / 4, "OUT", smallFontSize, smallFontLight, smallFont));
			addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, yPos), module, SIGNAL_OUTPUT));
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
				nvgFillColor(args.vg, nvgRGB(115, 115, 115));
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 14.00f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
				nvgRoundedRect(args.vg, RACK_GRID_WIDTH * 0.5f, RACK_GRID_WIDTH * 17.25f, RACK_GRID_WIDTH * 2, RACK_GRID_WIDTH * 3, 5);
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

Model* modelUtp = DigitalAtavism::createDaModel<DigitalAtavism::UT_P>("UT-P");
Model* modelUtox = DigitalAtavism::createDaModel<DigitalAtavism::UT_OX>("UT-OX");
