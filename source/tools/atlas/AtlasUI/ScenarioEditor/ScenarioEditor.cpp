#include "stdafx.h"

#include "ScenarioEditor.h"

#include "wx/glcanvas.h"
#include "CustomControls/SnapSplitterWindow/SnapSplitterWindow.h"

#include "GameInterface/MessagePasser.h"
#include "GameInterface/Messages.h"

#include "Sections/Map/Map.h"

#include "tools/Common/Tools.h"

//#define UI_ONLY

//////////////////////////////////////////////////////////////////////////

// TODO: move into another file
class Canvas : public wxGLCanvas
{
public:
	Canvas(wxWindow* parent, int* attribList)
		: wxGLCanvas(parent, -1, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS, _T("GLCanvas"), attribList),
		m_SuppressResize(true)
	{
	}

	void OnResize(wxSizeEvent&)
	{
		// Be careful not to send 'resize' messages to the game before we've
		// told it that this canvas exists
		if (! m_SuppressResize)
			ADD_COMMAND(ResizeScreen(GetClientSize().GetWidth(), GetClientSize().GetHeight()));
			// TODO: fix flashing
	}

	void InitSize()
	{
		m_SuppressResize = false;
		SetSize(320, 240);
	}

	bool KeyScroll(wxKeyEvent& evt, bool enable)
	{
		int dir;
		switch (evt.GetKeyCode())
		{
		case WXK_LEFT:  dir = AtlasMessage::mScrollConstant::LEFT; break;
		case WXK_RIGHT: dir = AtlasMessage::mScrollConstant::RIGHT; break;
		case WXK_UP:    dir = AtlasMessage::mScrollConstant::FORWARDS; break;
		case WXK_DOWN:  dir = AtlasMessage::mScrollConstant::BACKWARDS; break;
		case WXK_SHIFT: dir = -1; break;
		default: return false;
		}

		float speed = wxGetKeyState(WXK_SHIFT) ? 240.0f : 120.0f;

		if (dir == -1) // changed modifier keys - update all currently-scrolling directions
		{
			if (wxGetKeyState(WXK_LEFT))  ADD_INPUT(ScrollConstant(AtlasMessage::mScrollConstant::LEFT, speed));
			if (wxGetKeyState(WXK_RIGHT)) ADD_INPUT(ScrollConstant(AtlasMessage::mScrollConstant::RIGHT, speed));
			if (wxGetKeyState(WXK_UP))    ADD_INPUT(ScrollConstant(AtlasMessage::mScrollConstant::FORWARDS, speed));
			if (wxGetKeyState(WXK_DOWN))  ADD_INPUT(ScrollConstant(AtlasMessage::mScrollConstant::BACKWARDS, speed));
		}
		else
		{
			ADD_INPUT(ScrollConstant(dir, enable ? speed : 0.0f));
		}
		return true;
	}

	void OnKeyDown(wxKeyEvent& evt)
	{
		if (KeyScroll(evt, true))
			return;

		g_CurrentTool->OnKey(evt, ITool::KEY_DOWN);

		evt.Skip();
	}

	void OnKeyUp(wxKeyEvent& evt)
	{
		if (KeyScroll(evt, false))
			return;

		g_CurrentTool->OnKey(evt, ITool::KEY_UP);

		evt.Skip();
	}

	void OnMouse(wxMouseEvent& evt)
	{
		g_CurrentTool->OnMouse(evt);
	}

private:
	bool m_SuppressResize;
	DECLARE_EVENT_TABLE();
};
BEGIN_EVENT_TABLE(Canvas, wxGLCanvas)
	EVT_SIZE(Canvas::OnResize)
	EVT_KEY_DOWN(Canvas::OnKeyDown)
	EVT_KEY_UP(Canvas::OnKeyUp)
	
	EVT_LEFT_DOWN(Canvas::OnMouse)
	EVT_LEFT_UP  (Canvas::OnMouse)
	EVT_MOTION   (Canvas::OnMouse)
END_EVENT_TABLE()

//////////////////////////////////////////////////////////////////////////

enum
{
	ID_Quit = 1,
//	ID_New,
//	//	ID_Import,
//	//	ID_Export,
//	ID_Open,
//	ID_Save,
//	ID_SaveAs,
};

BEGIN_EVENT_TABLE(ScenarioEditor, wxFrame)
	EVT_CLOSE(ScenarioEditor::OnClose)
	EVT_TIMER(wxID_ANY, ScenarioEditor::OnTimer)

	EVT_MENU(ID_Quit, ScenarioEditor::OnQuit)
	EVT_MENU(wxID_UNDO, ScenarioEditor::OnUndo)
	EVT_MENU(wxID_REDO, ScenarioEditor::OnRedo)
END_EVENT_TABLE()


static AtlasWindowCommandProc g_CommandProc;
AtlasWindowCommandProc& ScenarioEditor::GetCommandProc() { return g_CommandProc; }

ScenarioEditor::ScenarioEditor()
: wxFrame(NULL, wxID_ANY, _("Atlas - Scenario Editor"), wxDefaultPosition, wxSize(1024, 768))
{
	//////////////////////////////////////////////////////////////////////////
	// Menu

	wxMenuBar* menuBar = new wxMenuBar;
	SetMenuBar(menuBar);

	wxMenu *menuFile = new wxMenu;
	menuBar->Append(menuFile, _("&File"));
	{
//		menuFile->Append(ID_New, _("&New"));
//		//		menuFile->Append(ID_Import, _("&Import..."));
//		//		menuFile->Append(ID_Export, _("&Export..."));
//		menuFile->Append(ID_Open, _("&Open..."));
//		menuFile->Append(ID_Save, _("&Save"));
//		menuFile->Append(ID_SaveAs, _("Save &As..."));
//		menuFile->AppendSeparator();//-----------
		menuFile->Append(ID_Quit,   _("E&xit"));
//		m_FileHistory.UseMenu(menuFile);//-------
//		m_FileHistory.AddFilesToMenu();
	}

//	m_menuItem_Save = menuFile->FindItem(ID_Save); // remember this item, to let it be greyed out
//	wxASSERT(m_menuItem_Save);

	wxMenu *menuEdit = new wxMenu;
	menuBar->Append(menuEdit, _("&Edit"));
	{
		menuEdit->Append(wxID_UNDO, _("&Undo"));
		menuEdit->Append(wxID_REDO, _("&Redo"));
	}

	GetCommandProc().SetEditMenu(menuEdit);
	GetCommandProc().Initialize();

	//////////////////////////////////////////////////////////////////////////
	// Main window

	SnapSplitterWindow* splitter = new SnapSplitterWindow(this);

	// Set up GL canvas:

	int glAttribList[] = {
		WX_GL_RGBA,
		WX_GL_DOUBLEBUFFER,
		WX_GL_DEPTH_SIZE, 24, // TODO: wx documentation doesn't say this is valid
		0
	};
	Canvas* canvas = new Canvas(splitter, glAttribList);
	// The canvas' context gets made current on creation; but it can only be
	// current for one thread at a time, and it needs to be current for the
	// thread that is doing the draw calls, so disable it for this one.
	wglMakeCurrent(NULL, NULL);

	// Set up sidebars:

	Sidebar* sidebar = new MapSidebar(splitter);

	// Build layout:

	splitter->SplitVertically(sidebar, canvas, 200);

	// Send setup messages to game engine:

#ifndef UI_ONLY
	ADD_COMMAND(SetContext(canvas->GetHDC(), canvas->GetContext()->GetGLRC()));

	ADD_COMMAND(CommandString("init"));

	canvas->InitSize();

	ADD_COMMAND(CommandString("render_enable"));
#endif

	// XXX
	USE_TOOL(AlterElevation);

	m_Timer.SetOwner(this);
	m_Timer.Start(50);
}


void ScenarioEditor::OnClose(wxCloseEvent&)
{
#ifndef UI_ONLY
	ADD_COMMAND(CommandString("shutdown"));
#endif
	ADD_COMMAND(CommandString("exit"));

	SetCurrentTool(NULL);

	// TODO: What if it's still rendering while we're destroying the canvas?
	Destroy();
}


void ScenarioEditor::OnTimer(wxTimerEvent&)
{
	// TODO: Improve timer stuff - smoother, etc
	static wxLongLong last = wxGetLocalTimeMillis();
	wxLongLong time = wxGetLocalTimeMillis();
	g_CurrentTool->OnTick((time-last).ToLong());
	last = time;
}

void ScenarioEditor::OnQuit(wxCommandEvent&)
{
	Close();
}

void ScenarioEditor::OnUndo(wxCommandEvent&)
{
	GetCommandProc().Undo();
}

void ScenarioEditor::OnRedo(wxCommandEvent&)
{
	GetCommandProc().Redo();
}

//////////////////////////////////////////////////////////////////////////

AtlasMessage::Position::Position(const wxPoint& pt)
{
	// TODO: convert to world space (via the engine)
	x = pt.x/32.f;
	y = 0.f;
	z = pt.y/32.f;
}
