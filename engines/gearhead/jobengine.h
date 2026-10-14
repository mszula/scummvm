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

#ifndef GEARHEAD_JOBENGINE_H
#define GEARHEAD_JOBENGINE_H

#include "gearhead/ui.h"

namespace Gearhead {

struct Job;
struct JobPhase;
class Sound;

/**
 * The job dialog (JobDialog, vt 0x4d0f2c): Overlays\Job\JobFrame.tga at
 * (175,90) with the customer's portrait, the phase text, difficulty, fee and
 * a hint line; buttons OK, CANCEL and RESTART.
 */
class JobDialog : public Dialog {
public:
	JobDialog();
	~JobDialog() override;

	/** JobDialog_init (0x447673) */
	void init();

	/** JobDialog_askRequest (0x447ae9): "Job Request"; true = OK */
	bool askRequest(Job &job, bool autoMode);
	/** JobDialog_askUpdate (0x447c5f): "Job Update"; true = continue (also when there is no hint) */
	bool askUpdate(Job &job, bool autoMode);
	/** JobDialog_showComplete (0x447d52): "Job Complete!" */
	void showComplete(Job &job);

protected:
	void drawContents() override;

private:
	/** JobDialog_show (0x4478e4) + the modal loop */
	int run(const JobPhase &phase);
	void onOk(Button *b);
	void onCancel(Button *b);

	ImageButton _restart, _cancel, _ok;
	Common::String _feeText, _difficultyText, _hintText;
	Common::Rect _portraitRect, _hintRect, _difficultyRect, _feeRect;
	const JobPhase *_phase;
	int _result; ///< g_JobDialogResult
};

/**
 * Job manager (JobEngine 0x294 bytes, g_JobEngine): offers jobs, runs the
 * "Job Help" tab of the workshop and pays the mechanic.
 */
class JobEngine : public JobDialog {
public:
	JobEngine();
	~JobEngine() override;

	/** JobEngine_init (0x44823d) */
	void init();

	/** JobEngine_update (0x448303): called about once a second by the workshop */
	void update();
	/** JobEngine_offerJob (0x4483e9) */
	void offerJob();
	/** The second half of JobEngine_offerJob: the request dialog for @p job, then the start */
	void offer(Job *job);
	/** Console: the job ends as completed at the next update, like the PgUp + Insert cheat */
	void requestCompletion() { _completionRequested = true; }
	/** JobEngine_startJob (0x44859a) */
	void startJob(Job *job);
	/** JobEngine_endJob (0x44867a) */
	void endJob(bool success, bool silent);

	Job *current() const { return _current; }
	ImageButton *tab() { return &_tab; }

private:
	/** JobEngine_onTabClick (0x44852d) */
	void onTabClick(Button *b);
	/** JobEngine_celebrate (0x448833): the car spins for 3 s */
	void celebrate();
	/** JobEngine_showTabPortrait (0x44892e) */
	void showTabPortrait(int phaseType);
	/** JobEngine_hideTab / showTab (0x44899f / 0x4489e6): redraw with another image, keep the state */
	void drawTab(int state);

	ImageButton _tab;
	Graphics::ManagedSurface *_tabImage;    ///< JobTab.tga with the portrait (up and down image)
	Graphics::ManagedSurface *_tabErase;    ///< WorkShop.tga under the tab (disabled image)
	Job *_current;
	Sound *_offerSound;
	Sound *_tabSound;
	int _savedCash;
	bool _cheatLatch; ///< g_JobCheatLatch
	bool _completionRequested;
};

} // End of namespace Gearhead

#endif // GEARHEAD_JOBENGINE_H
