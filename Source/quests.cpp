/**
 * @file quests.cpp
 *
 * Implementation of functionality for handling quests.
 */
#include "quests.h"

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/render/cel_render.hpp"
#include "engine/render/text_render.hpp"
#include "gendung.h"
#include "init.h"
#include "minitext.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "panels/ui_panels.hpp"
#include "stores.h"
#include "towners.h"
#include "trigs.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

bool QuestLogIsOpen;
std::optional<OwnedCelSprite> pQLogCel;
/** Contains the quests of the current game. */
Quest Quests[MAXQUESTS];
Point ReturnLvlPosition;
dungeon_type ReturnLevelType;
int ReturnLevel;

/** Contains the data related to each quest_id. */
QuestData QuestsData[] = {
	// clang-format off
	// _qdlvl,  _qdmultlvl, _qlvlt,          bookOrder,   _qdrnd, _qslvl,          isSinglePlayerOnly, _qdmsg,        _qlstr
	{       5,          -1, DTYPE_NONE,          5,      100,    SL_NONE,         true,               TEXT_INFRA5,   N_( /* TRANSLATORS: Quest Name Block */ "The Magic Rock")           },
	{       9,          -1, DTYPE_NONE,         10,      100,    SL_NONE,         true,               TEXT_MUSH8,    N_("Black Mushroom")           },
	{       4,          -1, DTYPE_NONE,          3,      100,    SL_NONE,         true,               TEXT_GARBUD1,  N_("Gharbad The Weak")         },
	{       8,          -1, DTYPE_NONE,          9,      100,    SL_NONE,         true,               TEXT_ZHAR1,    N_("Zhar the Mad")             },
	{      14,          -1, DTYPE_NONE,         21,      100,    SL_NONE,         true,               TEXT_VEIL9,    N_("Lachdanan")                },
	{      15,          -1, DTYPE_NONE,         23,      100,    SL_NONE,         false,              TEXT_VILE3,    N_("Diablo")                   },
	{       2,           2, DTYPE_NONE,          0,      100,    SL_NONE,         false,              TEXT_BUTCH9,   N_("The Butcher")              },
	{       4,          -1, DTYPE_NONE,          4,      100,    SL_NONE,         true,               TEXT_BANNER2,  N_("Ogden's Sign")             },
	{       7,          -1, DTYPE_NONE,          8,      100,    SL_NONE,         true,               TEXT_BLINDING, N_("Halls of the Blind")       },
	{       5,          -1, DTYPE_NONE,          6,      100,    SL_NONE,         true,               TEXT_BLOODY,   N_("Valor")                    },
	{      10,          -1, DTYPE_NONE,         11,      100,    SL_NONE,         true,               TEXT_ANVIL5,   N_("Anvil of Fury")            },
	{      13,          -1, DTYPE_NONE,         20,      100,    SL_NONE,         true,               TEXT_BLOODWAR, N_("Warlord of Blood")         },
	{       3,           3, DTYPE_CATHEDRAL,     2,      100,    SL_SKELKING,     false,              TEXT_KING2,    N_("The Curse of King Leoric") },
	{       2,          -1, DTYPE_CAVES,         1,      100,    SL_POISONWATER,  true,               TEXT_POISON3,  N_("Poisoned Water Supply")    },
	{       6,          -1, DTYPE_CATACOMBS,     7,      100,    SL_BONECHAMB,    true,               TEXT_BONER,    N_("The Chamber of Bone")      },
	{      15,          15, DTYPE_CATHEDRAL,    22,      100,    SL_VILEBETRAYER, false,              TEXT_VILE1,    N_("Archbishop Lazarus")       },
	{      17,          17, DTYPE_NONE,         17,      100,    SL_NONE,         false,              TEXT_GRAVE7,   N_("Grave Matters")            },
	{      9,            9, DTYPE_NONE,         12,      100,    SL_NONE,         false,              TEXT_FARMER1,  N_("Farmer's Orchard")         },
	{      17,          -1, DTYPE_NONE,         14,      100,    SL_NONE,         true,               TEXT_GIRL2,    N_("Little Girl")              },
	{      19,          -1, DTYPE_NONE,         16,      100,    SL_NONE,         true,               TEXT_TRADER,   N_("Wandering Trader")         },
	{      17,          17, DTYPE_NONE,         15,      100,    SL_NONE,         false,              TEXT_DEFILER1, N_("The Defiler")              },
	{      21,          21, DTYPE_NONE,         19,      100,    SL_NONE,         false,              TEXT_NAKRUL1,  N_("Na-Krul")                  },
	{      21,          -1, DTYPE_NONE,         18,      100,    SL_NONE,         true,               TEXT_CORNSTN,  N_("Cornerstone of the World") },
	{       9,           9, DTYPE_NONE,         13,      100,    SL_NONE,         false,              TEXT_JERSEY4,  N_( /* TRANSLATORS: Quest Name Block end*/ "The Jersey's Jersey")      },
	// clang-format on
};

namespace {

int WaterDone;

/** Indices of quests to display in quest log window. `FirstFinishedQuest` are active quests the rest are completed */
quest_id EncounteredQuests[MAXQUESTS];
/** Overall number of EncounteredQuests entries */
int EncounteredQuestCount;
/** First (nonselectable) finished quest in list */
int FirstFinishedQuest;
/** Currently selected quest list item */
int SelectedQuest;

constexpr Rectangle InnerPanel { { 32, 26 }, { 280, 300 } };
constexpr int LineHeight = 12;
constexpr int MaxSpacing = LineHeight * 2;
int ListYOffset;
int LineSpacing;
/** The number of pixels to move finished quest, to seperate them from the active ones */
int FinishedQuestOffset;

const char *const QuestTriggerNames[5] = {
	N_(/* TRANSLATORS: Quest Map*/ "King Leoric's Tomb"),
	N_(/* TRANSLATORS: Quest Map*/ "The Chamber of Bone"),
	N_(/* TRANSLATORS: Quest Map*/ "Maze"),
	N_(/* TRANSLATORS: Quest Map*/ "A Dark Passage"),
	N_(/* TRANSLATORS: Quest Map*/ "Unholy Altar")
};
/**
 * A quest group containing the three quests the Butcher,
 * Ogden's Sign and Gharbad the Weak, which ensures that exactly
 * two of these three quests appear in any single player game.
 */
int QuestGroup1[3] = { Q_BUTCHER, Q_LTBANNER, Q_GARBUD };
/**
 * A quest group containing the three quests Halls of the Blind,
 * the Magic Rock and Valor, which ensures that exactly two of
 * these three quests appear in any single player game.
 */
int QuestGroup2[3] = { Q_BLIND, Q_ROCK, Q_BLOOD };
/**
 * A quest group containing the three quests Black Mushroom,
 * Zhar the Mad and Anvil of Fury, which ensures that exactly
 * two of these three quests appear in any single player game.
 */
int QuestGroup3[3] = { Q_MUSHROOM, Q_ZHAR, Q_ANVIL };
/**
 * A quest group containing the two quests Lachdanan and Warlord
 * of Blood, which ensures that exactly one of these two quests
 * appears in any single player game.
 */
int QuestGroup4[2] = { Q_VEIL, Q_WARLORD };

/**
 * @brief There is no reason to run this, the room has already had a proper sector assigned
 */
void DrawButcher()
{
	Point position = SetPiece.position.megaToWorld() + Displacement { 3, 3 };
	DRLG_RectTrans({ position, { 7, 7 } });
}

void DrawSkelKing(quest_id q, Point position)
{
	Quests[q].position = position.megaToWorld() + Displacement { 12, 7 };
}

void DrawWarLord(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("Levels\\L4Data\\Warlord2.DUN");

	SetPiece = { position, { SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1]) } };

	PlaceDunTiles(dunData.get(), position, 6);
}

void DrawSChamber(quest_id q, Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("Levels\\L2Data\\Bonestr1.DUN");

	SetPiece = { position, { SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1]) } };

	PlaceDunTiles(dunData.get(), position, 3);

	Quests[q].position = position.megaToWorld() + Displacement { 6, 7 };
}

void DrawLTBanner(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("Levels\\L1Data\\Banner1.DUN");

	int width = SDL_SwapLE16(dunData[0]);
	int height = SDL_SwapLE16(dunData[1]);

	SetPiece = { position, { SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1]) } };

	const uint16_t *tileLayer = &dunData[2];

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			auto tileId = static_cast<uint8_t>(SDL_SwapLE16(tileLayer[j * width + i]));
			if (tileId != 0) {
				pdungeon[position.x + i][position.y + j] = tileId;
			}
		}
	}
}

/**
 * Close outer wall
 */
void DrawBlind(Point position)
{
	dungeon[position.x][position.y + 1] = 154;
	dungeon[position.x + 10][position.y + 8] = 154;
}

void DrawBlood(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("Levels\\L2Data\\Blood2.DUN");

	SetPiece = { position, { SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1]) } };

	PlaceDunTiles(dunData.get(), position, 0);
}

int QuestLogMouseToEntry()
{
	Rectangle innerArea = InnerPanel;
	innerArea.position += Displacement(GetLeftPanel().position.x, GetLeftPanel().position.y);
	if (!innerArea.Contains(MousePosition) || (EncounteredQuestCount == 0))
		return -1;
	int y = MousePosition.y - innerArea.position.y;
	for (int i = 0; i < FirstFinishedQuest; i++) {
		if ((y >= ListYOffset + i * LineSpacing)
		    && (y < ListYOffset + i * LineSpacing + LineHeight)) {
			return i;
		}
	}
	return -1;
}

void PrintQLString(const Surface &out, int x, int y, string_view str, bool marked, bool disabled = false)
{
	int width = GetLineWidth(str);
	x += std::max((257 - width) / 2, 0);
	if (marked) {
		CelDrawTo(out, GetPanelPosition(UiPanels::Quest, { x - 20, y + 13 }), CelSprite { *pSPentSpn2Cels }, PentSpn2Spin());
	}
	DrawString(out, str, { GetPanelPosition(UiPanels::Quest, { x, y }), { 257, 0 } }, disabled ? UiFlags::ColorWhitegold : UiFlags::ColorWhite);
	if (marked) {
		CelDrawTo(out, GetPanelPosition(UiPanels::Quest, { x + width + 7, y + 13 }), CelSprite { *pSPentSpn2Cels }, PentSpn2Spin());
	}
}

} // namespace

void InitQuests()
{
	QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_NONE;
	QuestDialogTable[TOWN_WITCH][Q_MUSHROOM] = TEXT_MUSH9;

	QuestLogIsOpen = false;
	WaterDone = 0;

	int initiatedQuests = 0;
	int q = 0;
	for (auto &quest : Quests) {
		quest._qidx = static_cast<quest_id>(q);
		auto &questData = QuestsData[q];
		q++;

		quest._qactive = QUEST_NOTAVAIL;
		quest.position = { 0, 0 };
		quest._qlvltype = questData._qlvlt;
		quest._qslvl = questData._qslvl;
		quest._qvar1 = 0;
		quest._qvar2 = 0;
		quest._qlog = false;
		quest._qmsg = questData._qdmsg;

		if (!gbIsMultiplayer) {
			quest._qlevel = questData._qdlvl;
			quest._qactive = QUEST_INIT;
		} else if (!questData.isSinglePlayerOnly) {
			quest._qlevel = questData._qdmultlvl;
			quest._qactive = QUEST_INIT;
			initiatedQuests++;
		}
	}

	if (!gbIsMultiplayer && *sgOptions.Gameplay.randomizeQuests) {
		// Quests are set from the seed used to generate level 16.
		InitialiseQuestPools(glSeedTbl[15], Quests);
	}

	if (gbIsSpawn) {
		for (auto &quest : Quests) {
			quest._qactive = QUEST_NOTAVAIL;
		}
	}

	if (Quests[Q_SKELKING]._qactive == QUEST_NOTAVAIL)
		Quests[Q_SKELKING]._qvar2 = 2;
	if (Quests[Q_ROCK]._qactive == QUEST_NOTAVAIL)
		Quests[Q_ROCK]._qvar2 = 2;
	Quests[Q_LTBANNER]._qvar1 = 1;
	if (gbIsMultiplayer)
		Quests[Q_BETRAYER]._qvar1 = 2;
}

void InitialiseQuestPools(uint32_t seed, Quest quests[])
{
	SetRndSeed(seed);
	if (GenerateRnd(2) != 0)
		quests[Q_PWATER]._qactive = QUEST_NOTAVAIL;
	else
		quests[Q_SKELKING]._qactive = QUEST_NOTAVAIL;

	// using int and not size_t here to detect negative values from GenerateRnd
	int randomIndex = GenerateRnd(sizeof(QuestGroup1) / sizeof(*QuestGroup1));

	if (randomIndex >= 0)
		quests[QuestGroup1[randomIndex]]._qactive = QUEST_NOTAVAIL;

	randomIndex = GenerateRnd(sizeof(QuestGroup2) / sizeof(*QuestGroup2));
	if (randomIndex >= 0)
		quests[QuestGroup2[randomIndex]]._qactive = QUEST_NOTAVAIL;

	randomIndex = GenerateRnd(sizeof(QuestGroup3) / sizeof(*QuestGroup3));
	if (randomIndex >= 0)
		quests[QuestGroup3[randomIndex]]._qactive = QUEST_NOTAVAIL;

	randomIndex = GenerateRnd(sizeof(QuestGroup4) / sizeof(*QuestGroup4));

	// always true, QuestGroup4 has two members
	if (randomIndex >= 0)
		quests[QuestGroup4[randomIndex]]._qactive = QUEST_NOTAVAIL;
}

void CheckQuests()
{
	if (gbIsSpawn)
		return;

	auto &quest = Quests[Q_BETRAYER];
	if (quest.IsAvailable() && gbIsMultiplayer && quest._qvar1 == 2) {
		AddObject(OBJ_ALTBOY, SetPiece.position.megaToWorld() + Displacement { 4, 6 });
		quest._qvar1 = 3;
		NetSendCmdQuest(true, quest);
	}

	if (gbIsMultiplayer) {
		return;
	}

	if (currlevel == quest._qlevel
	    && !setlevel
	    && quest._qvar1 >= 2
	    && (quest._qactive == QUEST_ACTIVE || quest._qactive == QUEST_DONE)
	    && (quest._qvar2 == 0 || quest._qvar2 == 2)) {
		// Move the quest trigger into world space, then spawn a portal at the same location
		quest.position = quest.position.megaToWorld();
		AddMissile(quest.position, quest.position, Direction::South, MIS_RPORTAL, TARGET_MONSTERS, MyPlayerId, 0, 0);
		quest._qvar2 = 1;
		if (quest._qactive == QUEST_ACTIVE && quest._qvar1 == 2) {
			quest._qvar1 = 3;
		}
	}

	if (quest._qactive == QUEST_DONE
	    && setlevel
	    && setlvlnum == SL_VILEBETRAYER
	    && quest._qvar2 == 4) {
		Point portalLocation { 35, 32 };
		AddMissile(portalLocation, portalLocation, Direction::South, MIS_RPORTAL, TARGET_MONSTERS, MyPlayerId, 0, 0);
		quest._qvar2 = 3;
	}

	if (setlevel) {
		if (setlvlnum == Quests[Q_PWATER]._qslvl
		    && Quests[Q_PWATER]._qactive != QUEST_INIT
		    && leveltype == Quests[Q_PWATER]._qlvltype
		    && ActiveMonsterCount == 4
		    && Quests[Q_PWATER]._qactive != QUEST_DONE) {
			Quests[Q_PWATER]._qactive = QUEST_DONE;
			PlaySfxLoc(IS_QUESTDN, MyPlayer->position.tile);
			LoadPalette("Levels\\L3Data\\L3pwater.pal", false);
			UpdatePWaterPalette();
			WaterDone = 32;
		}
	} else if (MyPlayer->_pmode == PM_STAND) {
		for (auto &quest : Quests) {
			if (currlevel == quest._qlevel
			    && quest._qslvl != 0
			    && quest._qactive != QUEST_NOTAVAIL
			    && MyPlayer->position.tile == quest.position) {
				if (quest._qlvltype != DTYPE_NONE) {
					setlvltype = quest._qlvltype;
				}
				StartNewLvl(MyPlayerId, WM_DIABSETLVL, quest._qslvl);
			}
		}
	}
}

bool ForceQuests()
{
	if (gbIsSpawn)
		return false;

	if (gbIsMultiplayer) {
		return false;
	}

	for (auto &quest : Quests) {
		if (quest._qidx != Q_BETRAYER && currlevel == quest._qlevel && quest._qslvl != 0) {
			int ql = quest._qslvl - 1;

			if (EntranceBoundaryContains(quest.position, cursPosition)) {
				InfoString = fmt::format(fmt::runtime(_(/* TRANSLATORS: Used for Quest Portals. {:s} is a Map Name */ "To {:s}")), _(QuestTriggerNames[ql]));
				cursPosition = quest.position;
				return true;
			}
		}
	}

	return false;
}

void CheckQuestKill(const Monster &monster, bool sendmsg)
{
	if (gbIsSpawn)
		return;

	Player &myPlayer = *MyPlayer;

	if (monster.MType->mtype == MT_SKING) {
		auto &quest = Quests[Q_SKELKING];
		quest._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::RestWellLeoricIllFindYourSon, 30);
		if (sendmsg)
			NetSendCmdQuest(true, quest);

	} else if (monster.MType->mtype == MT_CLEAVER) {
		auto &quest = Quests[Q_BUTCHER];
		quest._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::TheSpiritsOfTheDeadAreNowAvenged, 30);
		if (sendmsg)
			NetSendCmdQuest(true, quest);
	} else if (monster._uniqtype - 1 == UMT_GARBUD) { //"Gharbad the Weak"
		Quests[Q_GARBUD]._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::ImNotImpressed, 30);
	} else if (monster._uniqtype - 1 == UMT_ZHAR) { //"Zhar the Mad"
		Quests[Q_ZHAR]._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::ImSorryDidIBreakYourConcentration, 30);
	} else if (monster._uniqtype - 1 == UMT_LAZARUS) { //"Arch-Bishop Lazarus"
		auto &betrayerQuest = Quests[Q_BETRAYER];
		betrayerQuest._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::YourMadnessEndsHereBetrayer, 30);
		betrayerQuest._qvar1 = 7;
		auto &diabloQuest = Quests[Q_DIABLO];
		diabloQuest._qactive = QUEST_ACTIVE;

		if (gbIsMultiplayer) {
			for (int j = 0; j < MAXDUNY; j++) {
				for (int i = 0; i < MAXDUNX; i++) {
					if (dPiece[i][j] == 369) {
						trigs[numtrigs].position = { i, j };
						trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
						numtrigs++;
					}
				}
			}
			if (sendmsg) {
				NetSendCmdQuest(true, betrayerQuest);
				NetSendCmdQuest(true, diabloQuest);
			}
		} else {
			InitVPTriggers();
			betrayerQuest._qvar2 = 4;
			AddMissile({ 35, 32 }, { 35, 32 }, Direction::South, MIS_RPORTAL, TARGET_MONSTERS, MyPlayerId, 0, 0);
		}
	} else if (monster._uniqtype - 1 == UMT_WARLORD) { //"Warlord of Blood"
		Quests[Q_WARLORD]._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::YourReignOfPainHasEnded, 30);
	}
}

void DRLG_CheckQuests(Point position)
{
	for (auto &quest : Quests) {
		if (quest.IsAvailable()) {
			switch (quest._qidx) {
			case Q_BUTCHER:
				DrawButcher();
				break;
			case Q_LTBANNER:
				DrawLTBanner(position);
				break;
			case Q_BLIND:
				DrawBlind(position);
				break;
			case Q_BLOOD:
				DrawBlood(position);
				break;
			case Q_WARLORD:
				DrawWarLord(position);
				break;
			case Q_SKELKING:
				DrawSkelKing(quest._qidx, position);
				break;
			case Q_SCHAMB:
				DrawSChamber(quest._qidx, position);
				break;
			default:
				break;
			}
		}
	}
}

void SetReturnLvlPos()
{
	switch (setlvlnum) {
	case SL_SKELKING:
		ReturnLvlPosition = Quests[Q_SKELKING].position + Direction::SouthEast;
		ReturnLevel = Quests[Q_SKELKING]._qlevel;
		ReturnLevelType = DTYPE_CATHEDRAL;
		break;
	case SL_BONECHAMB:
		ReturnLvlPosition = Quests[Q_SCHAMB].position + Direction::SouthEast;
		ReturnLevel = Quests[Q_SCHAMB]._qlevel;
		ReturnLevelType = DTYPE_CATACOMBS;
		break;
	case SL_MAZE:
		break;
	case SL_POISONWATER:
		ReturnLvlPosition = Quests[Q_PWATER].position + Direction::SouthWest;
		ReturnLevel = Quests[Q_PWATER]._qlevel;
		ReturnLevelType = DTYPE_CATHEDRAL;
		break;
	case SL_VILEBETRAYER:
		ReturnLvlPosition = Quests[Q_BETRAYER].position + Direction::East;
		ReturnLevel = Quests[Q_BETRAYER]._qlevel;
		ReturnLevelType = DTYPE_HELL;
		break;
	case SL_NONE:
		break;
	}
}

void GetReturnLvlPos()
{
	if (Quests[Q_BETRAYER]._qactive == QUEST_DONE)
		Quests[Q_BETRAYER]._qvar2 = 2;
	ViewPosition = ReturnLvlPosition;
	currlevel = ReturnLevel;
	leveltype = ReturnLevelType;
}

void LoadPWaterPalette()
{
	if (!setlevel || setlvlnum != Quests[Q_PWATER]._qslvl || Quests[Q_PWATER]._qactive == QUEST_INIT || leveltype != Quests[Q_PWATER]._qlvltype)
		return;

	if (Quests[Q_PWATER]._qactive == QUEST_DONE)
		LoadPalette("Levels\\L3Data\\L3pwater.pal");
	else
		LoadPalette("Levels\\L3Data\\L3pfoul.pal");
}

void UpdatePWaterPalette()
{
	if (WaterDone > 0) {
		palette_update_quest_palette(WaterDone);
		WaterDone--;
		return;
	}
	palette_update_caves();
}

void ResyncMPQuests()
{
	if (gbIsSpawn)
		return;

	auto &kingQuest = Quests[Q_SKELKING];
	if (kingQuest._qactive == QUEST_INIT
	    && currlevel >= kingQuest._qlevel - 1
	    && currlevel <= kingQuest._qlevel + 1) {
		kingQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, kingQuest);
	}

	auto &butcherQuest = Quests[Q_BUTCHER];
	if (butcherQuest._qactive == QUEST_INIT
	    && currlevel >= butcherQuest._qlevel - 1
	    && currlevel <= butcherQuest._qlevel + 1) {
		butcherQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, butcherQuest);
	}

	auto &betrayerQuest = Quests[Q_BETRAYER];
	if (betrayerQuest._qactive == QUEST_INIT && currlevel == betrayerQuest._qlevel - 1) {
		betrayerQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, betrayerQuest);
	}
	if (betrayerQuest.IsAvailable())
		AddObject(OBJ_ALTBOY, SetPiece.position.megaToWorld() + Displacement { 4, 6 });

	auto &cryptQuest = Quests[Q_GRAVE];
	if (cryptQuest._qactive == QUEST_INIT && currlevel == cryptQuest._qlevel - 1) {
		cryptQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, cryptQuest);
	}

	auto &defilerQuest = Quests[Q_DEFILER];
	if (defilerQuest._qactive == QUEST_INIT && currlevel == defilerQuest._qlevel - 1) {
		defilerQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, defilerQuest);
	}

	auto &nakrulQuest = Quests[Q_NAKRUL];
	if (nakrulQuest._qactive == QUEST_INIT && currlevel == nakrulQuest._qlevel - 1) {
		nakrulQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, nakrulQuest);
	}
}

void ResyncQuests()
{
	if (gbIsSpawn)
		return;

	if (Quests[Q_LTBANNER].IsAvailable()) {
		if (Quests[Q_LTBANNER]._qvar1 == 1) {
			ObjChangeMapResync(
			    SetPiece.position.x + SetPiece.size.width - 2,
			    SetPiece.position.y + SetPiece.size.height - 2,
			    SetPiece.position.x + SetPiece.size.width + 1,
			    SetPiece.position.y + SetPiece.size.height + 1);
		}
		if (Quests[Q_LTBANNER]._qvar1 == 2) {
			ObjChangeMapResync(
			    SetPiece.position.x + SetPiece.size.width - 2,
			    SetPiece.position.y + SetPiece.size.height - 2,
			    SetPiece.position.x + SetPiece.size.width + 1,
			    SetPiece.position.y + SetPiece.size.height + 1);
			ObjChangeMapResync(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + (SetPiece.size.width / 2) + 2, SetPiece.position.y + (SetPiece.size.height / 2) - 2);
			for (int i = 0; i < ActiveObjectCount; i++)
				SyncObjectAnim(Objects[ActiveObjects[i]]);
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans({ SetPiece.position, { SetPiece.size.width / 2 + 4, SetPiece.size.height / 2 } });
			TransVal = tren;
		}
		if (Quests[Q_LTBANNER]._qvar1 == 3) {
			ObjChangeMapResync(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + SetPiece.size.width + 1, SetPiece.position.y + SetPiece.size.height + 1);
			for (int i = 0; i < ActiveObjectCount; i++)
				SyncObjectAnim(Objects[ActiveObjects[i]]);
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans({ SetPiece.position, { SetPiece.size.width / 2 + 4, SetPiece.size.height / 2 } });
			TransVal = tren;
		}
	}
	if (currlevel == Quests[Q_MUSHROOM]._qlevel) {
		if (Quests[Q_MUSHROOM]._qactive == QUEST_INIT && Quests[Q_MUSHROOM]._qvar1 == QS_INIT) {
			SpawnQuestItem(IDI_FUNGALTM, { 0, 0 }, 5, 1);
			Quests[Q_MUSHROOM]._qvar1 = QS_TOMESPAWNED;
		} else {
			if (Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE) {
				if (Quests[Q_MUSHROOM]._qvar1 >= QS_MUSHGIVEN) {
					QuestDialogTable[TOWN_WITCH][Q_MUSHROOM] = TEXT_NONE;
					QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_MUSH3;
				} else if (Quests[Q_MUSHROOM]._qvar1 >= QS_BRAINGIVEN) {
					QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_NONE;
				}
			}
		}
	}
	if (currlevel == Quests[Q_VEIL]._qlevel + 1 && Quests[Q_VEIL]._qactive == QUEST_ACTIVE && Quests[Q_VEIL]._qvar1 == 0) {
		Quests[Q_VEIL]._qvar1 = 1;
		SpawnQuestItem(IDI_GLDNELIX, { 0, 0 }, 5, 1);
	}
	if (setlevel && setlvlnum == SL_VILEBETRAYER) {
		if (Quests[Q_BETRAYER]._qvar1 >= 4)
			ObjChangeMapResync(1, 11, 20, 18);
		if (Quests[Q_BETRAYER]._qvar1 >= 6)
			ObjChangeMapResync(1, 18, 20, 24);
		if (Quests[Q_BETRAYER]._qvar1 >= 7)
			InitVPTriggers();
		for (int i = 0; i < ActiveObjectCount; i++)
			SyncObjectAnim(Objects[ActiveObjects[i]]);
	}
	if (currlevel == Quests[Q_BETRAYER]._qlevel
	    && !setlevel
	    && (Quests[Q_BETRAYER]._qvar2 == 1 || Quests[Q_BETRAYER]._qvar2 >= 3)
	    && (Quests[Q_BETRAYER]._qactive == QUEST_ACTIVE || Quests[Q_BETRAYER]._qactive == QUEST_DONE)) {
		Quests[Q_BETRAYER]._qvar2 = 2;
	}
}

void DrawQuestLog(const Surface &out)
{
	int l = QuestLogMouseToEntry();
	if (l >= 0) {
		SelectedQuest = l;
	}
	const auto x = InnerPanel.position.x;
	CelDrawTo(out, GetPanelPosition(UiPanels::Quest, { 0, 351 }), CelSprite { *pQLogCel }, 0);
	int y = InnerPanel.position.y + ListYOffset;
	for (int i = 0; i < EncounteredQuestCount; i++) {
		if (i == FirstFinishedQuest) {
			y += FinishedQuestOffset;
		}
		PrintQLString(out, x, y, _(QuestsData[EncounteredQuests[i]]._qlstr), i == SelectedQuest, i >= FirstFinishedQuest);
		y += LineSpacing;
	}
}

void StartQuestlog()
{

	auto sortQuestIdx = [](int a, int b) {
		return QuestsData[a].questBookOrder < QuestsData[b].questBookOrder;
	};

	EncounteredQuestCount = 0;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && quest._qlog) {
			EncounteredQuests[EncounteredQuestCount] = quest._qidx;
			EncounteredQuestCount++;
		}
	}
	FirstFinishedQuest = EncounteredQuestCount;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_DONE || quest._qactive == QUEST_HIVE_DONE) {
			EncounteredQuests[EncounteredQuestCount] = quest._qidx;
			EncounteredQuestCount++;
		}
	}

	std::sort(&EncounteredQuests[0], &EncounteredQuests[FirstFinishedQuest], sortQuestIdx);
	std::sort(&EncounteredQuests[FirstFinishedQuest], &EncounteredQuests[EncounteredQuestCount], sortQuestIdx);

	bool twoBlocks = FirstFinishedQuest != 0 && FirstFinishedQuest < EncounteredQuestCount;

	ListYOffset = 0;
	FinishedQuestOffset = !twoBlocks ? 0 : LineHeight / 2;

	int overallMinHeight = EncounteredQuestCount * LineHeight + FinishedQuestOffset;
	int space = InnerPanel.size.height;

	if (EncounteredQuestCount > 0) {
		int additionalSpace = space - overallMinHeight;
		int addLineSpacing = additionalSpace / EncounteredQuestCount;
		addLineSpacing = std::min(MaxSpacing - LineHeight, addLineSpacing);
		LineSpacing = LineHeight + addLineSpacing;
		if (twoBlocks) {
			int additionalSepSpace = additionalSpace - (addLineSpacing * EncounteredQuestCount);
			additionalSepSpace = std::min(LineHeight, additionalSepSpace);
			FinishedQuestOffset = std::max(4, additionalSepSpace);
		}

		int overallHeight = EncounteredQuestCount * LineSpacing + FinishedQuestOffset;
		ListYOffset += (space - overallHeight) / 2;
	}

	SelectedQuest = FirstFinishedQuest == 0 ? -1 : 0;
	QuestLogIsOpen = true;
}

void QuestlogUp()
{
	if (FirstFinishedQuest == 0) {
		SelectedQuest = -1;
	} else {
		SelectedQuest--;
		if (SelectedQuest < 0) {
			SelectedQuest = FirstFinishedQuest - 1;
		}
		PlaySFX(IS_TITLEMOV);
	}
}

void QuestlogDown()
{
	if (FirstFinishedQuest == 0) {
		SelectedQuest = -1;
	} else {
		SelectedQuest++;
		if (SelectedQuest == FirstFinishedQuest) {
			SelectedQuest = 0;
		}
		PlaySFX(IS_TITLEMOV);
	}
}

void QuestlogEnter()
{
	PlaySFX(IS_TITLSLCT);
	if (EncounteredQuestCount != 0 && SelectedQuest >= 0 && SelectedQuest < FirstFinishedQuest)
		InitQTextMsg(Quests[EncounteredQuests[SelectedQuest]]._qmsg);
	QuestLogIsOpen = false;
}

void QuestlogESC()
{
	int l = QuestLogMouseToEntry();
	if (l != -1) {
		QuestlogEnter();
	}
}

void SetMultiQuest(int q, quest_state s, bool log, int v1)
{
	if (gbIsSpawn)
		return;

	if (Quests[q]._qactive != QUEST_DONE) {
		if (s > Quests[q]._qactive)
			Quests[q]._qactive = s;
		if (log)
			Quests[q]._qlog = true;
		if (v1 > Quests[q]._qvar1)
			Quests[q]._qvar1 = v1;
	}
}

bool Quest::IsAvailable()
{
	if (setlevel)
		return false;
	if (currlevel != _qlevel)
		return false;
	if (_qactive == QUEST_NOTAVAIL)
		return false;
	if (gbIsMultiplayer && QuestsData[_qidx].isSinglePlayerOnly)
		return false;

	return true;
}

} // namespace devilution
