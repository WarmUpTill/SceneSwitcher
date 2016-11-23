using namespace std;

struct SceneSwitch
{
	OBSWeakSource scene;
	string window;
	OBSWeakSource transition;
	bool fullscreen;

	inline SceneSwitch(
		OBSWeakSource scene_, const char* window_, OBSWeakSource transition_, bool fullscreen_)
		: scene(scene_)
		, window(window_)
		, transition(transition_)
		, fullscreen(fullscreen_)
	{
	}
};

struct ExecutableSceneSwitch
{
	OBSWeakSource mScene;
	OBSWeakSource mTransition;
	QString mExe;
	bool mInFocus;

	inline ExecutableSceneSwitch(
		OBSWeakSource scene, OBSWeakSource transition, const QString& exe, bool inFocus)
		: mScene(scene)
		, mTransition(transition)
		, mExe(exe)
		, mInFocus(inFocus)
	{
	}
};

struct ScreenRegionSwitch
{
	OBSWeakSource scene;
	OBSWeakSource transition;
	int minX, minY, maxX, maxY;
	string regionStr;

	inline ScreenRegionSwitch(OBSWeakSource scene_, OBSWeakSource transition_, int minX_, int minY_,
		int maxX_, int maxY_, string regionStr_)
		: scene(scene_)
		, transition(transition_)
		, minX(minX_)
		, minY(minY_)
		, maxX(maxX_)
		, maxY(maxY_)
		, regionStr(regionStr_)
	{
	}
};

struct SceneRoundTripSwitch
{
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	int delay;
	string sceneRoundTripStr;

	inline SceneRoundTripSwitch(OBSWeakSource scene1_, OBSWeakSource scene2_,
		OBSWeakSource transition_, int delay_, string str)
		: scene1(scene1_)
		, scene2(scene2_)
		, transition(transition_)
		, delay(delay_)
		, sceneRoundTripStr(str)
	{
	}
};

struct SceneTransition
{
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	string sceneTransitionStr;

	inline SceneTransition(OBSWeakSource scene1_, OBSWeakSource scene2_, OBSWeakSource transition_,
		string sceneTransitionStr_)
		: scene1(scene1_)
		, scene2(scene2_)
		, transition(transition_)
		, sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct FileIOData
{
	bool readEnabled;
	string readPath;
	bool writeEnabled;
	string writePath;
};