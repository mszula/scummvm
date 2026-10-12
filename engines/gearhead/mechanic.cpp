/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/debug.h"
#include "common/stream.h"

#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/mechanic.h"
#include "gearhead/sound.h"
#include "gearhead/tlv.h"
#include "gearhead/ui.h"

namespace Gearhead {

static const SkillLevel kSkillTable[5] = {
	{ 0, "Learning", { 3.0f, 3.0f }, { 0.0f, 0.0f }, { 0.95f, 1.00f } },
	{ 1, "Novice", { 2.0f, 3.0f }, { 1.0f, 2.0f }, { 0.85f, 0.90f } },
	{ 2, "Handy", { 0.0f, 3.0f }, { 0.0f, 2.0f }, { 0.75f, 0.85f } },
	{ 3, "Expert", { 0.0f, 2.0f }, { 0.0f, 1.0f }, { 0.30f, 0.60f } },
	{ 4, "God-like", { 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.05f, 0.30f } }
};

const SkillLevel &skillLevel(int index) {
	return kSkillTable[CLIP(index, 0, 4)];
}

const char *skillName(int index) {
	return skillLevel(index).name;
}

Common::String formatWorkTime(int seconds) {
	const int h = seconds / 3600;
	const int m = (seconds - 3600 * h) / 60;
	const int s = seconds - 3600 * h - 60 * m;
	return Common::String::format("%02i:%02i:%02i", h, m, s);
}

Common::String formatDuration(int seconds) {
	const int h = seconds / 3600;
	const int m = (seconds - 3600 * h) / 60;
	const int s = seconds - 3600 * h - 60 * m;
	Common::String r;
	if (h != 0)
		r += Common::String::format("%i", h) + (h < 2 ? " hour and " : " hours and ");
	if (m != 0)
		r += Common::String::format("%i", m) + (m < 2 ? " minute and " : " minutes and ");
	r += Common::String::format("%i", s) + (s < 2 ? " second" : " seconds");
	return r;
}

// ---------------------------------------------------------------- PartsBank

PartsBank::~PartsBank() {
	for (uint i = 0; i < yard.size(); i++)
		delete yard[i];
	for (uint i = 0; i < bin.size(); i++)
		delete bin[i];
}

bool PartsBank::load(const TLVChunk &c) {
	// PartsBank_load (0x438bb2)
	for (uint i = 0; i < c.children().size(); i++) {
		const TLVChunk *ch = c.children()[i];
		switch (ch->id()) {
		case kTag_PBANK_AUTOID:
			modelId = ch->asInt();
			break;
		case kTag_PBANK_INBINPART:
		case kTag_PBANK_INYARDPART: {
			Part *p = loadPart(*ch);
			if (p)
				(ch->id() == kTag_PBANK_INBINPART ? bin : yard).push_back(p);
			break;
		}
		default:
			break;
		}
	}
	return true;
}

void PartsBank::save(TLVWriter &w) const {
	// PartsBank_save (0x438d1d)
	w.begin(kTag_MECH_PARTSBANK);
	w.writeInt(kTag_PBANK_AUTOID, modelId);
	for (uint i = 0; i < bin.size(); i++)
		savePart(w, kTag_PBANK_INBINPART, *bin[i]);
	for (uint i = 0; i < yard.size(); i++)
		savePart(w, kTag_PBANK_INYARDPART, *yard[i]);
	w.end();
}

static void removeFrom(Common::Array<Part *> &list, Part *p) {
	for (uint i = 0; i < list.size(); i++) {
		if (list[i] == p) {
			list.remove_at(i);
			return;
		}
	}
}

// ---------------------------------------------------------------- Mechanic

Mechanic::Mechanic() : _root(nullptr), _car(nullptr), _job(nullptr), _bank(nullptr) {
	init("Mechanic", 0, 0.0f, 0);
}

Mechanic::~Mechanic() {
	clear();
}

void Mechanic::clear() {
	for (uint i = 0; i < _banks.size(); i++)
		delete _banks[i];
	_banks.clear();
	_bank = nullptr;
	for (uint i = 0; i < _cars.size(); i++)
		delete _cars[i];
	_cars.clear();
	_rawDecals.clear();
	delete _root;
	_root = nullptr;
}

void Mechanic::init(const Common::String &name, int id, float cash, int skillIndex) {
	clear();
	_name = name;
	_id = id;
	_cash = cash;
	_skillIndex = skillIndex;
	_totalCareerAutos = 0;
	_totalWorkTime = 0;
	_lastAutoNum = -1;
	for (int i = 0; i < kCompletedJobWords; i++)
		_completedJobs[i] = 0;
	_autoJobRequest = true;
	_clockTime = 0;
	_skillCheckTime = 0;
	_car = nullptr;
	_job = nullptr;
}

bool Mechanic::load(Common::SeekableReadStream &stream) {
	TLVChunk *root = TLVReader::parseFile(stream, kTag_MECHANICFILE);
	if (!root)
		return false;
	const TLVChunk *body = root->find(kTag_MECHANIC);
	if (!body || !body->isList()) {
		delete root;
		return false;
	}

	init("Mechanic", 0, 0.0f, 0);
	_root = root;
	for (uint i = 0; i < body->children().size(); i++) {
		const TLVChunk *c = body->children()[i];
		switch (c->id()) {
		case kTag_MECH_NAME:
			_name = c->asString();
			break;
		case kTag_MECH_ID:
			_id = c->asInt();
			break;
		case kTag_MECH_CASH:
			_cash = c->asFloat();
			break;
		case kTag_MECH_SKILLINDEX:
			_skillIndex = c->asInt();
			break;
		case kTag_MECH_TOTALCAREERAUTOS:
			_totalCareerAutos = c->asInt();
			break;
		case kTag_MECH_TOTALWORKTIME:
			_totalWorkTime = c->asInt();
			break;
		case kTag_MECH_LASTAUTONUM:
			_lastAutoNum = c->asInt();
			break;
		case kTag_MECH_PARTSBANK: {
			PartsBank *b = new PartsBank();
			b->load(*c);
			_banks.push_back(b);
			break;
		}
		case kTag_MECH_AUTO: {
			// Auto_load: the model comes from CAR_ID (looked up first)
			CarModel *model = g_engine->_cars.find(c->getInt(kTag_CAR_ID, -1), true);
			if (!model) {
				warning("Couldn't find Auto with ModelNumber == %d", c->getInt(kTag_CAR_ID, -1));
				break;
			}
			CarInstance *car = new CarInstance(model, g_engine->_rnd);
			car->load(*c);
			_cars.push_back(car);
			break;
		}
		case kTag_MECH_DECAL:
			_rawDecals.push_back(c);
			break;
		case kTag_MECH_COMPLETEDJOBS: {
			const Common::Array<int32> words = c->asIntArray();
			for (uint w = 0; w < words.size() && w < kCompletedJobWords; w++)
				_completedJobs[w] = words[w];
			break;
		}
		case kTag_MECH_AUTOJOBREQUEST:
			// read as a byte by the original
			_autoJobRequest = c->data().size() > 0 && c->data()[0] != 0;
			break;
		default:
			// MECH_NUMCURAUTOS is ignored on read; unknown tags are skipped
			break;
		}
	}
	// The current car is the one with careerNum == MECH_LASTAUTONUM, the bank that of its model
	if (_lastAutoNum >= 0) {
		for (uint i = 0; i < _cars.size(); i++) {
			if (_cars[i]->_careerNum == _lastAutoNum) {
				_car = _cars[i];
				_bank = findBank(_car->model()->_id);
				break;
			}
		}
	}
	return true;
}

bool Mechanic::save(Common::WriteStream &stream) const {
	TLVWriter w;
	w.open(kTag_MECHANICFILE);
	w.begin(kTag_MECHANIC);
	w.writeString(kTag_MECH_NAME, _name);
	w.writeInt(kTag_MECH_ID, _id);
	w.writeFloat(kTag_MECH_CASH, _cash);
	w.writeInt(kTag_MECH_SKILLINDEX, _skillIndex);
	w.writeInt(kTag_MECH_NUMCURAUTOS, numCars());
	w.writeInt(kTag_MECH_TOTALCAREERAUTOS, _totalCareerAutos);
	w.writeInt(kTag_MECH_TOTALWORKTIME, _totalWorkTime);
	// Only written with a current car and no active job
	if (_car && !_job)
		w.writeInt(kTag_MECH_LASTAUTONUM, _car->_careerNum);
	for (uint i = 0; i < _banks.size(); i++)
		_banks[i]->save(w);
	for (uint i = 0; i < _cars.size(); i++)
		_cars[i]->save(w);
	for (uint i = 0; i < _rawDecals.size(); i++)
		w.writeChunk(*_rawDecals[i]);
	w.begin(kTag_MECH_COMPLETEDJOBS);
	for (int i = 0; i < kCompletedJobWords; i++)
		w.appendInt(_completedJobs[i]);
	w.end();
	w.writeInt(kTag_MECH_AUTOJOBREQUEST, _autoJobRequest ? 1 : 0);
	w.end();
	return w.close(stream);
}

void Mechanic::saveToDisk() const {
	Common::WriteStream *ws = g_engine->createMechanic(_id);
	if (!ws)
		return;
	save(*ws);
	ws->finalize();
	delete ws;
}

void Mechanic::saveIfReal() const {
	if (_id < kMechanicIdCarDev && !_job)
		saveToDisk();
}

bool Mechanic::reloadFromDisk() {
	debugC(1, kDebugGame, "Loading Mechanic from Disk: %d", _id);
	const int id = _id;
	const uint32 clock = _clockTime, skill = _skillCheckTime;
	Common::SeekableReadStream *s = g_engine->openMechanic(id);
	bool ok = s && load(*s);
	delete s;
	if (!ok)
		init(_name, id, _cash, _skillIndex);
	_clockTime = clock;
	_skillCheckTime = skill;
	return ok;
}

// ---------------------------------------------------------------- money

bool Mechanic::pay(int amount, bool sound) {
	if (!canAfford(amount)) {
		msgNoCash();
		return false;
	}
	_cash -= (float)amount;
	addRepairCost(amount);
	drawCash();
	if (sound)
		g_engine->_snd->play("misc\\cashreg.wav");
	return true;
}

void Mechanic::addRepairCost(int amount) {
	if (_car && !_car->_isTop)
		_car->_repairCost = (float)amount + _car->_repairCost;
}

void Mechanic::drawName() const {
	// Mechanic_drawName (0x459258)
	g_engine->_fonts[kFontStandard].drawText(*g_engine->_gfx, _name, Common::Rect(335, 15, 454, 33),
	                                          kAlignCenter, kAlignCenter, kKeyDefault, kPageBoth);
}

void Mechanic::drawCash() const {
	TextBox cash;
	cash.setRect(Common::Rect(515, 49, 635, 82));
	cash.draw(g_engine->_fonts[kFontCash], "$" + Common::String::format("%4.2f", _cash),
	          kAlignEnd, kAlignCenter, kKeyDefault, kPageFront);
}

// ---------------------------------------------------------------- cars

CarInstance *Mechanic::addNewCar(int carId) {
	CarModel *model = g_engine->_cars.find(carId, true);
	if (!model)
		return nullptr;
	MsvcRandom &rnd = g_engine->_rnd;
	CarInstance *car = new CarInstance(model, rnd);
	_cars.push_back(car);
	_car = car;
	// The three ranges are drawn fraction first, then spread, then centre
	const SkillLevel &s = skillLevel(_skillIndex);
	const float fraction = s.partFraction[0] + rnd.frand(s.partFraction[1] - s.partFraction[0]);
	const float spread = s.condSpread[0] + rnd.frand(s.condSpread[1] - s.condSpread[0]);
	const float centre = s.condCentre[0] + rnd.frand(s.condCentre[1] - s.condCentre[0]);
	car->randomize(centre, spread, fraction);
	ensureBank(car);
	car->_careerNum = _totalCareerAutos++;
	return car;
}

void Mechanic::buyCar(CarInstance *car, float price) {
	_car = car;
	_cash -= price;
	_cars.push_back(car);
	ensureBank(car);
	car->_careerNum = _totalCareerAutos;
	car->_purchaseCost = price;
	_totalCareerAutos++;
	g_engine->_snd->play("misc\\cashreg.wav");
}

void Mechanic::sellCurrentCar(float price) {
	for (uint i = 0; i < _cars.size(); i++) {
		if (_cars[i] == _car) {
			_cars.remove_at(i);
			break;
		}
	}
	// The sold car object goes to the auction; it is not deleted here
	_car = nullptr;
	_cash = price + _cash;
	g_engine->_snd->play("misc\\cashreg.wav");
}

PartsBank *Mechanic::findBank(int modelId) const {
	for (uint i = 0; i < _banks.size(); i++)
		if (_banks[i]->modelId == modelId)
			return _banks[i];
	return nullptr;
}

// ---------------------------------------------------------------- parts

void Mechanic::ensureBank(CarInstance *car) {
	const int modelId = car->model()->_id;
	if (findBank(modelId)) {
		_bank = findBank(modelId);
		return;
	}
	PartsBank *b = new PartsBank();
	b->modelId = modelId;
	_banks.push_back(b);
	_bank = b;
	seedBank(car, b);
}

void Mechanic::seedBank(CarInstance *car, PartsBank *bank) {
	// Mechanic_seedBank (0x458c25): the junkyard gets every part the car is
	// missing, then 0..7 random extra parts, all in random condition
	MsvcRandom &rnd = g_engine->_rnd;
	const CarModel *model = car->model();
	const Common::Array<PartDef *> &defs = model->_parts;
	for (uint i = 0; i < defs.size(); i++) {
		if (car->findPart(defs[i]->id) || defs[i]->special == 4)
			continue;
		const int cond = (int)rnd.frand(3.999f);
		bank->yard.push_back(createPart(model, defs[i], cond, rnd));
	}
	const int extra = (int)rnd.frand(7.0f);
	for (int i = 0; i < extra && !defs.empty(); i++) {
		// Rand_float includes its upper bound: index == count reads past the
		// end of the list in the original (1 in 32768); clamped here
		const int idx = MIN<int>((int)rnd.frand((float)defs.size()), defs.size() - 1);
		if (defs[idx]->special == 4)
			continue;
		const int cond = (int)rnd.frand(3.999f);
		bank->yard.push_back(createPart(model, defs[idx], cond, rnd));
	}
}

void Mechanic::restockYard() {
	if (!_bank || !_car)
		return;
	MsvcRandom &rnd = g_engine->_rnd;
	const CarModel *model = _car->model();
	const int n = (int)rnd.frand(4.0f);
	for (int i = 0; i < n; i++) {
		int coin = (int)rnd.frand(1.999f);
		if (_bank->yard.empty())
			coin = 1;
		if (coin == 0) {
			const int idx = (int)rnd.frand((float)_bank->yard.size() - 0.01f);
			delete _bank->yard[idx];
			_bank->yard.remove_at(idx);
		} else {
			for (int t = 0; t < 100; t++) {
				const int idx = (int)rnd.frand((float)model->_parts.size() - 0.01f);
				const PartDef *pd = model->_parts[idx];
				if (pd->special == 4)
					continue;
				const int cond = (int)rnd.frand(3.999f);
				_bank->yard.push_back(createPart(model, pd, cond, rnd));
				break;
			}
		}
	}
}

bool Mechanic::buyYardPart(Part *part) {
	if (!_bank)
		return false;
	bool inYard = false;
	for (uint i = 0; i < _bank->yard.size(); i++)
		if (_bank->yard[i] == part)
			inYard = true;
	if (!inYard || !pay((int)part->junkPrice(), false))
		return false;
	removeFrom(_bank->yard, part);
	_bank->bin.push_back(part);
	return true;
}

void Mechanic::returnYardPart(Part *part) {
	// Mechanic_pay with a negative amount always succeeds: cash up, repair cost down
	pay(-(int)part->junkPrice(), false);
	if (_bank) {
		removeFrom(_bank->bin, part);
		_bank->yard.push_back(part);
	}
}

bool Mechanic::removePart(Part *part) {
	AssemblyError err;
	if (!_car || !_bank || !_car->canRemove(part, err))
		return false;
	g_engine->_snd->play("rattle\\rattle12.wav");
	removeFrom(_car->_parts, part);
	_bank->bin.push_back(part);
	return true;
}

bool Mechanic::attachPart(Part *part) {
	AssemblyError err;
	if (!_car || !_bank || !_car->canInstall(part, err))
		return false;
	removeFrom(_bank->bin, part);
	_car->_parts.push_back(part);
	return true;
}

void Mechanic::repairPart(Part *part) {
	if (part->condition == 0)
		return;
	Sound *s = g_engine->_snd->get("misc\\repair.wav");
	s->play();
	while (s->isPlaying() && g_engine->pollOnly()) {
	}
	part->condition = 3;
}

void Mechanic::sellPart(Part *part) {
	_cash = part->junkPrice() + _cash;
	// Original bug: the money received is added to the car's repair cost
	if (!g_engine->fixOriginalBugs())
		addRepairCost((int)part->junkPrice());
	Sound *s = g_engine->_snd->get("misc\\toilet2.wav");
	s->play();
	while (s->isPlaying() && g_engine->pollOnly()) {
	}
	drawCash();
	if (_bank) {
		removeFrom(_bank->bin, part);
		_bank->yard.push_back(part);
	}
}

void Mechanic::updateCurrentCar(int region) {
	// Auto_showForWorkshop (0x43dfc3)
	if (_car)
		_car->showRegion(region, true);
}

// ---------------------------------------------------------------- messages

void Mechanic::msgNoCash() {
	g_engine->_msgBox->message("* Bummer *", "You don't have the cash!", kDlgButtonOk);
}

void Mechanic::msgCarLotFull() {
	g_engine->_msgBox->message("* Car Lot Full *", "Your Car Lot is full. Auction some of your cars to make room.", kDlgButtonOk);
}

void Mechanic::msgAssemblyError(const AssemblyError &err) {
	// Mechanic_msgAssemblyError (0x4598e1): only the first listed part is named
	if (!_car || err.ids.empty())
		return;
	const PartDef *pd = _car->model()->findPart(err.ids[0]);
	const Common::String name = pd ? pd->name : Common::String();
	const bool plural = !name.empty() && (name.lastChar() == 's' || name.lastChar() == 'S');
	const char *verb = plural ? " are " : " is ";
	Common::String text;
	switch (err.code) {
	case AssemblyError::kAlreadyInstalled:
		if (!plural) {
			text = "There's already a";
			const char c = name.empty() ? 0 : name[0];
			if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
				text += "n";
			text += " " + name + " on the car. Remove that first.";
		} else {
			text = "There's already some " + name + " on the car. Remove those first.";
		}
		break;
	case AssemblyError::kMissingDeps:
		text = "You can't attach that part until the " + name + verb + "attached.";
		break;
	case AssemblyError::kBlocked:
		text = "You can't remove that part until the " + name + verb + "taken off.";
		break;
	case AssemblyError::kBroken:
		text = "That part's too messed up. It won't last ten minutes. You'll have to scrap it.";
		break;
	case AssemblyError::kReplaces:
		text = "That part replaces the " + name + ", which" + verb + "already attached.";
		break;
	default:
		break;
	}
	g_engine->_msgBox->message("Assembly Error!", text, kDlgButtonOk);
}

void Mechanic::msgCarComplete() {
	if (!_car || _id >= kMechanicIdCarDev || _job)
		return;
	g_engine->_jukebox->playJingle();
	const Common::String text = "Congratulations! You've fixed this baby up to Top Condition. And it only took you " +
	                            formatDuration(_car->_repairTimeMs / 1000) + ". You spent a total of $" +
	                            Common::String::format("%4.2f", _car->_repairCost) + ".";
	g_engine->_msgBox->message("Car Complete", text, kDlgButtonOk);
}

// ---------------------------------------------------------------- clock and skill

void Mechanic::update() {
	const uint32 now = g_engine->gameTimeMs();
	if (_clockTime + 1000 < now) {
		// The fraction is dropped every update, so the clock runs a little slow
		_totalWorkTime = (int)(_totalWorkTime + (now - _clockTime) / 1000.0);
		_clockTime = now;
	}
	// Mechanic_tickSkillAndTime (0x45939f)
	if (_skillCheckTime + 2000 < now) {
		_skillCheckTime = now;
		checkSkill();
	}
	if (_car && !_car->_isTop)
		_car->_repairTimeMs += now - g_engine->prevGameTimeMs();
}

void Mechanic::checkSkill() {
	const int n = countCompletedJobs();
	if (n == 9)
		advanceSkill(1);
	if (n == 20)
		advanceSkill(2);
	if (_cash >= 15000.0f)
		advanceSkill(2);
	if (numCars() > 4)
		advanceSkill(2);
	if (_cash >= 35000.0f)
		advanceSkill(3);
	if (numCars() > 10)
		advanceSkill(3);
	if (_cash >= 120000.0f)
		advanceSkill(4);
}

void Mechanic::advanceSkill(int level) {
	if (level <= _skillIndex || level > _skillIndex + 1)
		return;
	const Common::String oldName = skillName(_skillIndex);
	_skillIndex = MIN(_skillIndex + 1, 4);
	g_engine->_msgBox->message("Skill Advance", "Congratulations! You just moved up from " + oldName +
	                           " skill to the " + skillName(_skillIndex) + " skill level!", kDlgButtonOk);
	if (_skillIndex != 1)
		g_engine->_msgBox->message("Available Cars", "You'll have access to more cars in the Auction, and there may be new jobs to complete.", kDlgButtonOk);
}

bool Mechanic::isJobCompleted(int jobId) const {
	if (jobId < 0 || jobId >= kCompletedJobWords * 32)
		return false;
	return (_completedJobs[jobId / 32] >> (jobId % 32)) & 1;
}

void Mechanic::setJobCompleted(int jobId) {
	if (jobId >= 0 && jobId < kCompletedJobWords * 32)
		_completedJobs[jobId / 32] |= (int32)(1u << (jobId % 32));
	if (!_autoJobRequest)
		return;
	const int n = countCompletedJobs();
	if (n == 1) {
		g_engine->_msgBox->message("Congratulations", "Way to go. You've just completed your first job as a Virtual Mechanic.", kDlgButtonOk);
		g_engine->_msgBox->message("The Future", "With any luck, more jobs will roll in until you can afford to buy your own cars.", kDlgButtonOk);
	}
	if (n > 8)
		_autoJobRequest = false;
}

int Mechanic::countCompletedJobs() const {
	// Original quirk: only bits 0..15 of every word are counted
	const int bits = g_engine->fixOriginalBugs() ? 32 : 16;
	int n = 0;
	for (int w = 0; w < kCompletedJobWords; w++)
		for (int b = 0; b < bits; b++)
			if ((_completedJobs[w] >> b) & 1)
				n++;
	return n;
}

// ---------------------------------------------------------------- MechSummary

bool MechSummary::load(Common::SeekableReadStream &stream) {
	TLVChunk *root = TLVReader::parseFile(stream, kTag_MECHANICFILE);
	if (!root)
		return false;
	const TLVChunk *body = root->find(kTag_MECHANIC);
	if (!body) {
		delete root;
		return false;
	}
	name = body->getString(kTag_MECH_NAME);
	id = body->getInt(kTag_MECH_ID);
	skill = skillName(body->getInt(kTag_MECH_SKILLINDEX));
	workTime = formatWorkTime(body->getInt(kTag_MECH_TOTALWORKTIME));
	cars = Common::String::format("%i", body->getInt(kTag_MECH_NUMCURAUTOS));
	cash = Common::String::format("%6.2f", body->getFloat(kTag_MECH_CASH));
	delete root;
	return true;
}

} // End of namespace Gearhead
