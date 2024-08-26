#include "plugin.hpp"

namespace DigitalAtavism {
namespace math {

template<typename T>
static constexpr auto sqr(const T& v) -> decltype(v * v) { return v * v; }

template<unsigned N>
inline unsigned rnd()
{
	return std::rand() % (N + 1);
}

inline float frnd()
{
	return rnd<10000>() / 10000.f;
}

} // namespace math

namespace dsp {

struct HrEnvelope
{
	enum
	{
		STAGES_HOLD,
		STAGES_RELEASE,
		STAGES_COUNT,
		STAGES_START = 0,
	};

	int env_time{};
	int env_stage{ STAGES_COUNT };
	int env_length[STAGES_COUNT] = {};
	float punch{};

	void start(float sampleRate, float holdTime, float releaseTime, float punchAmount = 0.0f)
	{
		env_time = 0;
		env_stage = STAGES_START;
		env_length[STAGES_HOLD] = static_cast<int>(holdTime * sampleRate);
		env_length[STAGES_RELEASE] = static_cast<int>(releaseTime * sampleRate);
		punch = punchAmount;
	}

	void stop()
	{
		env_stage = STAGES_COUNT;
	}

	bool isActive() const { return env_stage < STAGES_COUNT; }

	float process(float deltaTime = 0.01f)
	{
		while (env_stage < STAGES_COUNT)
		{
			const auto& current_env_length = env_length[env_stage];
			if (++env_time <= current_env_length)
			{
				const auto fraction = static_cast<float>(current_env_length - env_time) / current_env_length;
				switch (env_stage)
				{
				case STAGES_HOLD:
					return 1.0f + fraction * 2.0f * punch;
				case STAGES_RELEASE:
					return fraction;
				default:
					return 0;
				}
			}

			env_time -= current_env_length;
			++env_stage;
		}

		return 0;
	}
};

struct TimedTrigger
{
	float remaining = 0.f;
	bool state = true;

	void reset()
	{
		remaining = 0.f;
		state = true;
	}

	bool process(float deltaTime)
	{
		const auto previousState = state;
		if (isActive())
			remaining -= deltaTime;

		state = !isActive();
		return !previousState && state;
	}

	void trigger(float duration)
	{
		if (duration > remaining)
			remaining = duration;
	}

	bool isActive() const
	{
		return remaining > 0.f;
	}
};

} // namespace dsp

namespace gui {

template<int _alignment>
struct TextLabel : widget::TransparentWidget
{
	enum { alignment = _alignment };

	std::string text;
	int fontSize;
	NVGcolor color;
	std::shared_ptr<Font> font;

	TextLabel(int x, int y, const char* str, int fontSize, NVGcolor color, const char* font)
		: text{ str }
		, fontSize{ fontSize }
		, color{ color }
		, font{ APP->window->loadFont(asset::plugin(pluginInstance, font)) }
	{
		box.pos = Vec(x, y);
	}

	virtual void draw(const DrawArgs &args) override
	{
		if (font->handle < 0)
			return;

		bndSetFont(font->handle);

		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);
		nvgTextAlign(args.vg, _alignment);

		nvgFillColor(args.vg, color);

		nvgBeginPath(args.vg);
		nvgText(args.vg, 0, 0, text.c_str(), NULL);

		bndSetFont(APP->window->uiFont->handle);
	}
};

struct BaseModuleWidget : app::ModuleWidget
{
	virtual void draw(const DrawArgs& args) override;
	void addName(const char* name, const char* digitalAtavism = "Digital Atavism");
	virtual NVGcolor getBackgroundFillColor() = 0;
	virtual NVGcolor getBackgroundStrokeColor() = 0;

	const char* font = "res/fonts/Orbitron-VariableFont_wght.ttf";
	const int fontSize = 14;

	const char* smallFont = "res/fonts/Ubuntu-Bold.ttf";
	const int smallFontSize = 8;

	const NVGcolor smallFontDark = nvgRGB(54, 54, 54);
	const NVGcolor smallFontLight = nvgRGB(222, 222, 222);
};

template<typename MyModule>
struct BasicMenuItem : MenuItem
{
	MyModule* module{};

	template <class TMenuItem = BasicMenuItem, typename... Args>
	static TMenuItem * createMenuItem(MyModule* module, const std::string& text, const std::string& rightText, Args... args)
	{
		TMenuItem* o = new TMenuItem(args...);
		o->module = module;
		o->text = text;
		o->rightText = rightText;
		return o;
	}
};

} // namespace gui

template<class TModule>
rack::plugin::Model* createDaModel(const std::string& slug)
{
	return createModel<TModule, typename TModule::Widget>(slug);
}

template<unsigned bits>
float quantize(float value)
{
#if __cplusplus >= 201700L
#define DA_IF_CONSTEXPR if constexpr
#else
#define DA_IF_CONSTEXPR if
#endif
	DA_IF_CONSTEXPR (bits == 0)
	{
		return 0.f;
	}
	else DA_IF_CONSTEXPR (bits == 1)
	{
		return value >= 0.f ? 1.f : -1.f;
	}
	else DA_IF_CONSTEXPR (bits > 64)
	{
		return value;
	}
	else if (value >= 1.f)
	{
		return 1.f;
	}
	else if (value <= -1.f)
	{
		return -1.f;
	}
	else
	{
		constexpr std::int64_t max = (1 << (bits - 1)) - 1;
		return static_cast<float>(static_cast<std::int64_t>(value * max)) / max;
	}
#undef DA_IF_CONSTEXPR
}

float quantize(float value, unsigned bits);

} // namespace DigitalAtavism

namespace da = DigitalAtavism;
