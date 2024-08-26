#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	// p->addModel(modelMyModule);
	p->addModel(modelBlip);
	p->addModel(modelCoin);
	p->addModel(modelDTrig);
	p->addModel(modelHit);
	p->addModel(modelUtp);
	p->addModel(modelUtox);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
