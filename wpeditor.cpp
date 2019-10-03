
#include "wpeditor.h"

#define IMAGECLASS WPEditCtrlImg
#define IMAGEFILE <WordProcCtrl/wordprocctrl.iml>
//#include <Draw/iml_header.h>
#include <Draw/iml.h>


#define TFILE <WordProcCtrl/wordprocctrl.t>
#include <Core/t.h>

bool WPFontHeight::Key(dword key, int count)
{
	if (key==K_ENTER) { if (!IsError(GetData())) WhenSelect(); return true; }
	return WithDropChoice<EditDouble>::Key(key, count);
}

double WPEditor::DotToPt(int dt) { return (7200*minmax(dt, 8, 8000)/600/10/10.0); } //??wtf 

int WPEditor::PtToDot(double pt) { return int((600*pt+71)/72); }

struct WPEditPageDraw : public PageDraw
{
	virtual Draw& Page(int _page);
	Draw&              w;
	int                page;
	int                x, y;
	Size               size;
	WPEditPageDraw(Draw& w) : w(w) { w.Begin(); w.Begin(); page=(-1); }
	~WPEditPageDraw() { w.End(); w.End(); }
};

Draw& WPEditPageDraw::Page(int _page)
{
	if (page==_page) return w;
	page = _page;
	w.End();
	w.End();
	if (size.cy<INT_MAX) w.Clipoff(0, y+((size.cy+3)*page)+2, 9999, size.cy);
	else w.Offset(0, y + 2);
	w.Offset(x, 0);
	return w;
}

Rect WPEditor::GetTextRect() const
{
	Size sz = GetSize();
	if (sz.cy<Zx(16)) sz.cy=Zx(16);
	if (sz.cx<Zx(80)) return RectC(0, 0, Zx(48), max(sz.cy, Zy(16)));
	int cx=zoom*(sz.cx-(2*viewborder))/100;
	return RectC((sz.cx-cx)/2, 0, cx, sz.cy);
}

Zoom WPEditor::GetZoom() const { return Zoom(GetTextRect().Width(), pagesz.cx); }

Size WPEditor::GetZoomedPage() const { return Size(GetTextRect().Width(), (pagesz.cy==INT_MAX)?INT_MAX:(GetZoom()*pagesz.cy)); }

int  WPEditor::GetPosY(PageY py) const { return (py.page*(GetZoomedPage().cy+3)+(py.y*GetZoom())+2); }

PageY WPEditor::GetPageY(int y) const
{
	PageY py;
	int q=(GetZoomedPage().cy+3);
	py.page=(y/q);
	py.y=(((y%q)-2)/GetZoom());
	return py;
}

static void WPsPaintHotSpot(Draw& w, int x, int y) { w.DrawRect(x+1, y+1, 6, 6, LtRed); DrawFrame(w, x, y, 8, 8, SColorText); }

void WPEditor::Paint(Draw& w)
{
	Size sz=GetSize();
	p_size=sz;
	Rect tr=GetTextRect();
	Zoom zoom=GetZoom();
	w.DrawRect(sz, White);
	PageY py=text.GetHeight(pagesz);
	
	{
		WPEditPageDraw pw(w);
		pw.x=tr.left;
		pw.y=-sb;
		pw.size=GetZoomedPage();
		if (pagesz.cy==INT_MAX)
		{
			pw.size.cy=INT_MAX;
			if (viewborder) { DrawFrame(w, tr.left-1, ((int)sb)?(-1):0, pw.size.cx+4, 9999, SColorShadow); }
		}
		else if (viewborder)
		{
			for(int i=0;i<=py.page;i++) DrawFrame(w, tr.left-1, i*(pw.size.cy+3)+1-sb, pw.size.cx+4, pw.size.cy+2, SColorShadow);
		}
		PaintInfo pi = paint_info;
		pi.context = context;
		pi.zoom = zoom;
		pi.top = GetPageY(sb);
		pi.bottom = GetPageY(sb + sz.cy);
		pi.usecache = true;
		pi.sizetracking = sizetracking;
		pi.showcodes = showcodes;
		pi.showlabels = !IsNull(showcodes) && viewborder >= 16;
		if (spellcheck) pi.spellingchecker = SpellParagraph;
		if (IsSelection())
		{
			if (tablesel) { pi.tablesel = tablesel; pi.cells = cells; }
			else { pi.sell = begtabsel ? -1 : min(cursor, anchor); pi.selh = max(cursor, anchor); }
		}
		text.Paint(pw, pagesz, pi);
	}
	
	w.DrawRect(tr.left, GetPosY(py) - sb, 20, 3, showcodes);
	if (objectpos >= 0)
	{
		Rect r = objectrect;
		r.Offset(tr.left, -sb);
		DrawFrame(w, r, SColorText);
		r.Deflate(1);
		DrawFatFrame(w, r, Blend(SColorHighlight, SColorLight), 2);
		r.Deflate(2);
		DrawFrame(w, r, SColorText);
		r.Deflate(1);
		WPsPaintHotSpot(w, r.left + r.Width() / 2 - 3, r.bottom - 7);
		WPsPaintHotSpot(w, r.right - 7, r.bottom - 7);
		WPsPaintHotSpot(w, r.right - 7, r.top + r.Height() / 2 - 3);
		w.Clip(r);
		w.End();
	}
	else if (paintcarect) w.DrawRect(GetCaretRect(), InvertColor);
	if (!IsNull(dropcaret)) DrawTiles(w, dropcaret.OffsetedVert(-sb), CtrlImg::checkers());
	scroller.Set(sb);
}

int WPEditor::GetHotSpot(Point p) const
{
	if (objectpos<0) return (-1);
	Rect r = objectrect;
	r.Offset(GetTextRect().left, -sb);
	r.Deflate(4, 4);
	if (RectC(r.right-7, r.bottom-7, 8, 12).Contains(p)) return 0;
	if (RectC(r.left+(r.Width()/2)-3, r.bottom-7, 12, 12).Contains(p)) return 1;
	if (RectC(r.right-7, r.top+(r.Height()/2)-3, 12, 8).Contains(p)) return 2;
	return (-1);
}

void WPEditor::SetZsc() { zsc=(int)sb/GetZoom(); }

void WPEditor::SetSb() { sb.SetTotal(GetPosY(text.GetHeight(pagesz))+10); }

void WPEditor::Scroll() { scroller.Scroll(*this, GetSize(), sb); PlaceCaret(); }

void WPEditor::EndSizeTracking() { if (sizetracking) { sizetracking=false; Refresh(); }}

void WPEditor::FixObjectRect()
{
	if (objectpos >= 0)
	{
		Rect r = GetObjectRect(objectpos);
		if (r != objectrect) { objectrect=r; Refresh(objectrect); }
	}
}

WPEditor& WPEditor::Floating(double zoomlevel_)
{
	floating_zoom=zoomlevel_;
	RefreshLayoutDeep();
	return *this;
}

void WPEditor::Layout()
{
	Size sz = GetTextRect().GetSize();
	if (!IsNull(floating_zoom))
	{
		Zoom m=GetRichTextStdScreenZoom();
		SetPage(Size(int(1 / floating_zoom * m.d / m.m * sz.cx), INT_MAX)); ///??????? operator precedence???
	}
	sb.SetPage((sz.cy>10)?(sz.cy-4):sz.cy);
	SetupRuler();
	SetSb();
	sb = zsc * GetZoom();
	PlaceCaret();
	if (GetSize()!=p_size) { sizetracking=true; KillSetTimeCallback(250, THISBACK(EndSizeTracking), TIMEID_ENDSIZETRACKING); }
	FixObjectRect();
}

Rect WPEditor::GetCaretRect(const RichCaret& pr) const
{
	Zoom zoom=GetZoom();
	Rect tr=GetTextRect();
	Rect r=RectC(	((pr.left*zoom)+tr.left),
					(GetPosY(pr)+(pr.lineascent-pr.caretascent)*zoom-sb),
					((overwrite && (GetChar(cursor)!='\n'))?(pr.Width()*zoom):(((pr.caretascent+pr.caretdescent)*zoom)>20)?2:1),
					((pr.caretascent+pr.caretdescent)*zoom));
	if (r.right>tr.right) return Rect(tr.right-r.Width(), r.top, tr.right, r.bottom);
	return r;
}

Rect WPEditor::GetCaretRect() const { return GetCaretRect(text.GetCaret(cursor, pagesz)); }

Rect WPEditor::PlaceCaret()
{
	Zoom zoom = GetZoom();
	Rect rr = Rect(zoom*cursorc.left, GetPosY(cursorc), zoom*cursorc.right, GetPosY(PageY(cursorc.page, cursorc.bottom)));
	if (objectpos >= 0) { KillCaret(); return rr; }
	if (!IsNull(objectrect)) { objectrect=Null; Refresh(); }
	if(IsSelection()) KillCaret();
	else SetCaret(GetCaretRect(cursorc));
	return rr;
}

void WPEditor::SetupRuler()
{
	Zoom zoom=GetZoom();
	static struct Tl
	{
		double grid;
		int    numbers;
		double numbermul;
		int    marks;
	}
	tl[] = {
		{ 25, 20, 25, 4 },
		{ 600 / 72 * 4, 9, 4, 1000 },
		{ 600 / 10, 10, 1 / 10.0, 5 },
		{ 600 / 25.4, 10, 1, 5 },
		{ 600 / 25.4, 10, 1 / 10.0, 5 },
	};
	const Tl& q=tl[unit];
	ruler.SetLayout(GetTextRect().left + zoom * cursorc.textpage.left, cursorc.textpage.Width(),
	                zoom, q.grid, q.numbers, q.numbermul, q.marks);
}

void WPEditor::SetupUnits()
{
	WithUnitLayout<TopWindow> d;
	CtrlLayoutOKCancel(d, t_("Units"));
	d.accels <<= THISBACK(StyleKeys);
	for (int i=1;i<=10;i++) d.zoom.Add(10*i, Format(t_("%d%% of width"), 10 * i));
	CtrlRetriever r;
	r(d.unit, unit)(d.showcodes, showcodes)(d.zoom, zoom);
	if (d.Execute()==IDOK) { r.Retrieve(); Refresh(); FinishNF(); }
}

void WPEditor::ZoomView(int d) { zoom=clamp(zoom+d*10, 10, 100); Refresh(); FinishNF(); }

int  WPEditor::GetNearestPos(int x, PageY py)
{
	int c=text.GetPos(x, py, pagesz);
	String dummy;
	RichPos p=text.GetRichPos(c);
	if ((c>=(text.GetLength()-1))||(c<0)||p.object||p.field||p.table&&((p.posincell==0)||(p.posincell==p.celllen))) return c;
	Rect r1 = text.GetCaret(c, pagesz);
	Rect r2 = text.GetCaret(c + 1, pagesz);
	return (((r1.top==r2.top)&&((x-r1.left)>(r2.left-x)))?(c+1):c);
}

int WPEditor::GetX(int x) { return ((x-GetTextRect().left)/GetZoom()); }

int WPEditor::GetSnapX(int x) { return (GetX(x)/32*32); }

void WPEditor::GetPagePoint(Point p, PageY& py, int& x) { py=GetPageY(p.y + sb); x=GetX(p.x); }

int  WPEditor::GetMousePos(Point p)
{
	PageY py;
	int x;
	GetPagePoint(p, py, x);
	return GetNearestPos(x, py);
}

Rect WPEditor::GetObjectRect(int pos) const
{
	Zoom zoom=GetZoom();
	RichCaret pr=text.GetCaret(pos, pagesz);
	Rect r=Rect(	zoom * pr.left,
					GetPosY(PageY(pr.page, pr.top + pr.lineascent - pr.objectcy + pr.objectyd)),
					zoom * pr.right,
					GetPosY(PageY(pr.page, pr.top + pr.lineascent + pr.objectyd)));
	return r;
}

bool WPEditor::Print()
{
	text.SetFooter(footer);
	text.PrintNoLinks(nolinks);
	return UPP::Print(text, pagesz, cursorc.page);
}

struct WPDisplayFont : public Display
{
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		Font fnt;
		fnt.Face((int)q);
		fnt.Height(r.Height() - Zy(4));
		w.DrawRect(r, paper);
		w.DrawText(r.left, r.top+(r.Height()-fnt.Info().GetHeight())/2, Font::GetFaceName((int)q), fnt, ink);
	}
};

struct WPValueDisplayFont : public Display {
	void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		w.DrawRect(r, paper);
		w.DrawText(r.left, r.top+(r.Height()-StdFont().Info().GetHeight())/2, Font::GetFaceName((int)q), StdFont(), ink);
	}
};

void WPEditor::Clear(bool bSetModified)
{
	undo.Clear();
	redo.Clear();
	text.Clear();
	Reset();
	RichPara h;
	h.format.language = GetCurrentLanguage();
	text.Cat(h);
	Refresh();
	Finish();
	ReadStyles();
	if (bSetModified)
	{
		SetModify();
		modified=true;
	}
	zsc=0;
}

void WPEditor::SetupLanguage(Vector<int>&& _lng)
{
	Vector<int> &lng=const_cast<Vector<int>&>(_lng);
	Sort(lng);
	language.ClearList();
	for (int i=0; i<lng.GetCount(); i++) language.Add(lng[i], ((lng[i])?LNGAsText(lng[i]):String(t_("None"))));
}

void WPEditor::Pick(RichText pick_ t)
{
	Clear();
	text=pick(t);
	if (text.GetPartCount()==0) text.Cat(RichPara());
	ReadStyles();
	EndSizeTracking();
	SetupLanguage(text.GetAllLanguages());
	Move(0);
	Update();
}

Value WPEditor::GetData() const { return AsQTF(text); }

void  WPEditor::SetData(const Value& v) { Pick(ParseQTF((String)v, 0, context)); }

void  WPEditor::Serialize(Stream& s)
{
	int version = 0;
	s / version;
	String h;
	if (s.IsStoring()) h=AsQTF(text);
	s % h;
	if (s.IsLoading()) Pick(ParseQTF(h, 0, context));
}

int WPEditor::fh[] = { 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 42, 48, 60, 72, 0 };

WPEditor& WPEditor::FontFaces(const Vector<int>& ff)
{
	ffs <<= ff;
	face.ClearList();
	for(int i = 0; i < ff.GetCount(); i++) face.Add(ff[i]);
	return *this;
}

void SetupFaceList(DropList& face)
{
	face.ValueDisplay(Single<WPValueDisplayFont>());
	face.SetDisplay(Single<WPDisplayFont>());
}

void WPEditor::SpellCheck() { spellcheck=!spellcheck; Refresh(); RefreshBar(); }

void WPEditor::SerializeSettings(Stream& s)
{
	int version = 3;
	s / version;
	s % unit;
	s % showcodes;
	if (version>=1) s % zoom;
	s % spellcheck;
	s % findreplace.find;
	findreplace.find.SerializeList(s);
	s % findreplace.replace;
	findreplace.replace.SerializeList(s);
	s % findreplace.wholeword;
	s % findreplace.ignorecase;
	RefreshBar();
	imagefs.Serialize(s);
	if (version>=3)
	{
		for (int i = 0; i < 20; i++)
		{
			StyleKey& k = stylekey[i];
			s % k.styleid % k.stylename % k.face % k.height % k.ink % k.paper;
		}
	}
}

void WPEditor::Reset()
{
	undoserial = 0;
	incundoserial = false;

	objectpos = -1;
	objectrect = Null;
	sizetracking = true;

	anchor = cursor = 0;
	gx = 0;
	oselh = osell = 0;

	RichPara::Format pmt;
	formatinfo.Set(pmt);

	tabmove.table = 0;
}

WPEditor::UndoInfo WPEditor::PickUndoInfo()
{
	UndoInfo f;
	f.undoserial = undoserial;
	f.undo = pick(undo);
	f.redo = pick(redo);
	Clear();
	return f;
}

void WPEditor::SetPickUndoInfo(UndoInfo pick_ f)
{
	undoserial = f.undoserial;
	incundoserial = true;
	undo = pick(f.undo);
	redo = pick(f.redo);
	Finish();
}

void WPEditor::PosInfo::Serialize(Stream& s)
{
	int version = 2;
	s / version;
	s % cursor % anchor % zsc % begtabsel;
	if (version==0) zsc = 0;
}

WPEditor::PosInfo WPEditor::GetPosInfo() const
{
	PosInfo f;
	f.cursor=cursor;
	f.anchor=anchor;
	f.begtabsel=begtabsel;
	f.zsc = zsc;
	return f;
}

void WPEditor::SetPosInfo(const PosInfo& f)
{
	int l=text.GetLength();
	cursor=min(l, f.cursor);
	anchor=min(l, f.anchor);
	begtabsel=f.begtabsel;
	if (begtabsel) anchor=0;
	Finish();
	zsc=f.zsc;
	Layout();
}

void WPEditor::DoRefreshBar() { WhenRefreshBar(); }

void WPEditor::RefreshBar()
{
	KillTimeCallback(TIMEID_REFRESHBAR);
	SetTimeCallback(0, THISBACK(DoRefreshBar), TIMEID_REFRESHBAR);
}

void StdLinkDlg(String& s, WString&) { EditText(s, t_("Hyperlink"), t_("Hyperlink"), CharFilterAscii128, 1000); }

void StdLabelDlg(String& s) { EditText(s, t_("Paragraph label"), t_("Label"), CharFilterAscii128, 1000); }

void StdIndexEntryDlg(String& s) { EditText(s, t_("Index Entry"), t_("Index entry"), CharFilterAscii128, 1000); }

WPEditor::WPEditor()
{
	floating_zoom = Null;

	Unicode();
	BackPaint();

	viewborder = Zx(16);

	face.NoWantFocus();
	height.NoWantFocus();
	style.NoWantFocus();
	language.NoWantFocus();

	setstyle = &style.InsertButton(0).SetMonoImage(CtrlImg::smallleft()).Tip(t_("Store as style"));
	setstyle->WhenClick = THISBACK(SetStyle);
	style.InsertButton(0).SetMonoImage(WPEditCtrlImg::ManageStyles()).Tip(t_("Style manager")).WhenClick = THISBACK(Styles);
	style.Tip(t_("Style"));

	style <<= THISBACK(Style);
	
	WhenBar = THISBACK(StdBar);

	pagesz = Size(3968, 6074);
	unit = WP_UNIT_POINT;
	zoom = 100;
	Clear();

	context = NULL;
	
	nolinks = false;

	showcodes = LtBlue;
	spellcheck = true;

	overwrite = false;

	sb.WhenScroll = THISBACK(Scroll);
	sb.SetLine(16);
	Layout();
	SetSb();

	adjustunits.Image(WPEditCtrlImg::AdjustUnits());
	adjustunits <<= THISBACK(SetupUnits);
	ruler.Add(adjustunits.RightPosZ(4, 16).VSizePosZ(2, 2));

	undosteps = 500;

	AddFrame(ViewFrame());
	AddFrame(ruler);
	AddFrame(sb);
	RefreshBar();

	ruler.WhenBeginTrack = THISBACK(BeginRulerTrack);
	ruler.WhenTrack = THISBACK(RulerTrack);
	ruler.WhenEndTrack = THISBACK(ReadFormat);
	ruler.WhenLeftDown = THISBACK(AddTab);
	ruler.WhenRightDown = THISBACK(TabMenu);

	SetupFaceList(face);
	face <<= THISBACK(SetFace);
	face.Tip(t_("Font face"));
	Vector<int> ff;
	ff.Add(Font::ARIAL);
	ff.Add(Font::ROMAN);
	ff.Add(Font::COURIER);
	FontFaces(ff);

	language <<= THISBACK(SetLanguage);
	language.Tip(t_("Language"));
	language.WhenClick = THISBACK(Language);
	language.Add(0, t_("None"));

	for (int i=0; fh[i]; i++) height.AddList(fh[i]);
	height.WhenSelect = THISBACK(SetHeight);
	height.Tip(t_("Font height"));

	hyperlink <<= THISBACK(Hyperlink);
	hyperlink.NoWantFocus();
	label <<= THISBACK(Label);
	indexentry << THISBACK(IndexEntry);
	indexentry.NoWantFocus();

	gotolabel.SetMonoImage(WPEditCtrlImg::GoTo());
	label.AddFrame(gotolabel);
	gotolabel.Tip(t_("Go to label"));
	gotolabel <<= THISBACK(GotoLbl);
	gotolabel.NoWantFocus();

	gotoentry.SetMonoImage(WPEditCtrlImg::GoTo());
	indexentry.AddFrame(gotoentry);
	gotoentry.Tip(t_("Go to index entry"));
	gotoentry <<= THISBACK(GotoEntry);

	gototable.Normal();
	gototable.AddIndex();
	gototable.AddIndex();

	gototable.WhenSelect = THISBACK(Goto);

	ink.ColorImage(WPEditCtrlImg::InkColor()).NullImage(WPEditCtrlImg::NullInkColor()).StaticImage(WPEditCtrlImg::ColorA());
	ink.NotNull();
	paper.ColorImage(WPEditCtrlImg::PaperColor()).NullImage(WPEditCtrlImg::NullPaperColor()).StaticImage(WPEditCtrlImg::ColorA());
	ink <<= THISBACK(SetInk);
	ink.Tip(t_("Text color"));
	paper <<= THISBACK(SetPaper);
	paper.Tip(t_("Background color"));

	ReadStyles();

	paintcarect = false;

	CtrlLayoutOKCancel(findreplace, t_("Find / Replace"));
	findreplace.cancel <<= callback(&findreplace, &TopWindow::Close);
	findreplace.ok <<= THISBACK(Find);
	findreplace.amend <<= THISBACK(Replace);
	notfoundfw = found = false;
	findreplace.NoCenter();

	WhenHyperlink = callback(StdLinkDlg);
	WhenLabel = callback(StdLabelDlg);
	WhenIndexEntry = callback(StdIndexEntryDlg);

	p_size = Size(-1, -1);

	useraction=modified=false;
	ClearModify();
	Finish();
	
	imagefs.Type("Images (*.png *.gif *.jpg *.bmp)", "*.png *.gif *.jpg *.bmp");
	
	singleline = false;
	
	clipzoom = Zoom(1, 1);
	
	bullet_indent = 150;
	
	persistent_findreplace = true;
	
	ignore_physical_size = false;
}

WPEditor::~WPEditor() {}

//void WPEditor::SetDefaultFace(const String sf, int h)
//{
//
////	auto lt=[](const String &l, const String &r)->bool{ return (ToLower(l)<ToLower(r)); };
////	std::map<String, int, decltype(lt) > fi(lt);
////	Vector<int> VF;
////	for (int i=0;i<Font::GetFaceCount();i++) { fi[Font::GetFaceName(i)]=i; }
////	for (auto p:fi) VF.push_back(p.second);
//	
////	int x=Font::FindFaceNameIndex(sf); //def ret=0 => stdfont
////	face.SetData(x);
////	SetFace();
//
//
////	int x=face.FindValue(sf); if (x>=0) { face.SetIndex(x); SetFace(); }
////	if ((h>0)&&(h<=72)) { height <<= h; SetHeight(); }
//
//}



void WPEditCtrlWithToolBar::TheBar(Bar& bar) { DefaultBar(bar, extended); }

void WPEditCtrlWithToolBar::RefreshBar() { toolbar.Set(THISBACK(TheBar)); }

void WPEditor::EvaluateFields()
{
	WhenStartEvaluating();
	text.EvaluateFields(vars);
	Finish();
}

WPEditCtrlWithToolBar::WPEditCtrlWithToolBar()
{
	InsertFrame(0, toolbar);
	WhenRefreshBar = callback(this, &WPEditCtrlWithToolBar::RefreshBar);
	extended = true;
}

//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

void WPEditor::UserAction()
{
	useraction = true;
}

Event<>  WPEditor::User(Event<>  cb)
{
	cb << THISBACK(UserAction);
	return cb;
}

#define USERBACK(x) User(THISBACK(x))
#define USERBACK1(x, y) User(THISBACK1(x, y))

void WPEditor::StyleTool(Bar& bar, int width)
{
	bar.Add(!IsReadOnly(), style, width);
}

void WPEditor::FaceTool(Bar& bar, int width)
{
	bar.Add(!IsReadOnly(), face, width);
}

void WPEditor::HeightTool(Bar& bar, int width)
{
	bar.Add(!IsReadOnly(), height, width);
}

void WPEditor::BoldTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Bold"),
	        formatinfo.charvalid & RichText::BOLD ? WPEditCtrlImg::Bold() : WPEditCtrlImg::BoldMixed(),
	        USERBACK(Bold))
	   .Check(formatinfo.IsBold() && (formatinfo.charvalid & RichText::BOLD))
	   .Key(key);
}

void WPEditor::ItalicTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Italic"),
            formatinfo.charvalid & RichText::ITALIC ? WPEditCtrlImg::Italic() : WPEditCtrlImg::ItalicMixed(),
	        USERBACK(Italic))
	   .Check(formatinfo.IsItalic() && (formatinfo.charvalid & RichText::ITALIC))
	   .Key(key);
}

void WPEditor::UnderlineTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Underline"),
	        formatinfo.charvalid & RichText::UNDERLINE ? WPEditCtrlImg::Underline()
	                                               : WPEditCtrlImg::UnderlineMixed(),
	        USERBACK(Underline))
	   .Check(formatinfo.IsUnderline() && (formatinfo.charvalid & RichText::UNDERLINE))
	   .Key(key);
}

void WPEditor::StrikeoutTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Strikeout"),
	        formatinfo.charvalid & RichText::STRIKEOUT ? WPEditCtrlImg::Strikeout()
	                                               : WPEditCtrlImg::StrikeoutMixed(),
	        USERBACK(Strikeout))
	   .Check(formatinfo.IsStrikeout() && (formatinfo.charvalid & RichText::STRIKEOUT))
	   .Key(key);
}

void WPEditor::CapitalsTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Capitals"),
	        formatinfo.charvalid & RichText::CAPITALS  ? WPEditCtrlImg::Capitals()
	                                               : WPEditCtrlImg::CapitalsMixed(),
	        USERBACK(Capitals))
	   .Check(formatinfo.capitals && (formatinfo.charvalid & RichText::CAPITALS));
}

void WPEditor::SuperscriptTool(Bar& bar, dword key)
{
	int i = formatinfo.charvalid & RichText::SSCRIPT ? formatinfo.sscript : 0;
	bar.Add(!IsReadOnly(), t_("Superscript"),
	        formatinfo.charvalid & RichText::SSCRIPT ? WPEditCtrlImg::SuperScript()
	                                             : WPEditCtrlImg::SuperScriptMixed(),
			USERBACK1(SetScript, i == 1 ? 0 : 1))
	   .Check(i == 1);
}

void WPEditor::SubscriptTool(Bar& bar, dword key)
{
	int i = formatinfo.charvalid & RichText::SSCRIPT ? formatinfo.sscript : 0;
	bar.Add(!IsReadOnly(), t_("Subscript"),
	        formatinfo.charvalid & RichText::SSCRIPT ? WPEditCtrlImg::SubScript()
	                                             : WPEditCtrlImg::SubScriptMixed(),
			USERBACK1(SetScript, i == 2 ? 0 : 2))
	   .Check(i == 2);
}

void WPEditor::FontTools(Bar& bar)
{
	FaceTool(bar);
	bar.Gap(4);
	HeightTool(bar);
	bar.Gap();
	BoldTool(bar);
	ItalicTool(bar);
	UnderlineTool(bar);
	StrikeoutTool(bar);
	CapitalsTool(bar);
	SuperscriptTool(bar);
	SubscriptTool(bar);
}

void WPEditor::InkTool(Bar& bar)
{
	bar.Add(!IsReadOnly(), ink);
}

void WPEditor::PaperTool(Bar& bar)
{
	bar.Add(!IsReadOnly(), paper);
}

void WPEditor::LanguageTool(Bar& bar, int width)
{
	if(!fixedlang)
		bar.Add(!IsReadOnly(), language, width);
}

void WPEditor::SpellCheckTool(Bar& bar)
{
	bar.Add(t_("Show spelling errors"), WPEditCtrlImg::SpellCheck(), USERBACK(SpellCheck))
	   .Check(spellcheck);
}

String PlusKeyDesc(const char *text, dword key)
{
	String r = text;
	if(key)
		r << ' ' << GetKeyDesc(key);
	return r;
}

void Setup(DataPusher& b, const char *tip, const char *dtip, dword key)
{
	const char *s = tip ? tip : dtip;
	b.Tip(PlusKeyDesc(s, key));
	b.NullText(s, StdFont().Italic(), SColorDisabled());
}

void WPEditor::IndexEntryTool(Bar& bar, int width, dword key, const char *tip)
{
	bar.Add(!IsReadOnly(), indexentry, width);
	Setup(indexentry, tip, t_("Index entry"), key);
	bar.AddKey(key, USERBACK(IndexEntry));
}

void WPEditor::HyperlinkTool(Bar& bar, int width, dword key, const char *tip)
{
	bar.Add(!IsReadOnly(), hyperlink, width);
	Setup(hyperlink, tip, t_("Hyperlink"), key);
	bar.AddKey(key, USERBACK(Hyperlink));
}

void WPEditor::LabelTool(Bar& bar, int width, dword key, const char *tip)
{
	bar.Add(!IsReadOnly(), label, width);
	Setup(label, tip, t_("Paragraph label"), key);
	bar.AddKey(key, USERBACK(Label));
}

void WPEditor::LeftTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::ALIGN ? formatinfo.align : Null;
	bar.Add(!IsReadOnly(), t_("Left"), WPEditCtrlImg::Left(), USERBACK(AlignLeft))
	   .Check(a == ALIGN_LEFT)
	   .Key(key);
}

void WPEditor::RightTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::ALIGN ? formatinfo.align : Null;
	bar.Add(!IsReadOnly(), t_("Right"), WPEditCtrlImg::Right(), USERBACK(AlignRight))
	   .Check(a == ALIGN_RIGHT)
	   .Key(key);
}

void WPEditor::CenterTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::ALIGN ? formatinfo.align : Null;
	bar.Add(!IsReadOnly(), t_("Center"), WPEditCtrlImg::Center(), USERBACK(AlignCenter))
	   .Check(a == ALIGN_CENTER)
	   .Key(key);
}

void WPEditor::JustifyTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::ALIGN ? formatinfo.align : Null;
	bar.Add(!IsReadOnly(), t_("Justify"), WPEditCtrlImg::Justify(), USERBACK(AlignJustify))
	   .Check(a == ALIGN_JUSTIFY)
	   .Key(key);
}

void  WPEditor::RoundBulletTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::BULLET ? formatinfo.bullet : Null;
	bar.Add(!IsReadOnly(), t_("Round bullet"), WPEditCtrlImg::RoundBullet(),
	        USERBACK1(SetBullet, RichPara::BULLET_ROUND))
	   .Check(a == RichPara::BULLET_ROUND)
	   .Key(key);
}

void  WPEditor::RoundWhiteBulletTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::BULLET ? formatinfo.bullet : Null;
	bar.Add(!IsReadOnly(), t_("White round bullet"), WPEditCtrlImg::RoundWhiteBullet(),
	        USERBACK1(SetBullet, RichPara::BULLET_ROUNDWHITE))
	   .Check(a == RichPara::BULLET_ROUNDWHITE)
	   .Key(key);
}

void  WPEditor::BoxBulletTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::BULLET ? formatinfo.bullet : Null;
	bar.Add(!IsReadOnly(), t_("Box bullet"), WPEditCtrlImg::BoxBullet(),
	        USERBACK1(SetBullet, RichPara::BULLET_BOX))
	   .Check(a == RichPara::BULLET_BOX)
	   .Key(key);
}

void  WPEditor::BoxWhiteBulletTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::BULLET ? formatinfo.bullet : Null;
	bar.Add(!IsReadOnly(), t_("White box bullet"), WPEditCtrlImg::BoxWhiteBullet(),
	        USERBACK1(SetBullet, RichPara::BULLET_BOXWHITE))
	   .Check(a == RichPara::BULLET_BOXWHITE)
	   .Key(key);
}

void  WPEditor::TextBulletTool(Bar& bar, dword key)
{
	int a = formatinfo.paravalid & RichText::BULLET ? formatinfo.bullet : Null;
	bar.Add(!IsReadOnly(), t_("Text bullet"), WPEditCtrlImg::TextBullet(),
	        USERBACK1(SetBullet, RichPara::BULLET_TEXT))
	   .Check(a == RichPara::BULLET_TEXT)
	   .Key(key);
}

void WPEditor::ParaFormatTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Paragraph format.."), WPEditCtrlImg::ParaFormat(), USERBACK(ParaFormat))
	   .Key(key);
}

void WPEditor::ToParaTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly() && IsSelection() && !tablesel, t_("To single paragraph"),
	        WPEditCtrlImg::ToPara(), USERBACK(ToPara))
	   .Key(key);
}

void WPEditor::ParaTools(Bar& bar)
{
	LeftTool(bar);
	CenterTool(bar);
	RightTool(bar);
	JustifyTool(bar);
	bar.Gap();
	RoundBulletTool(bar);
	RoundWhiteBulletTool(bar);
	BoxBulletTool(bar);
	BoxWhiteBulletTool(bar);
	TextBulletTool(bar);
	bar.Gap();
	ToParaTool(bar);
	bar.Gap();
	ParaFormatTool(bar);
}

void WPEditor::UndoTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly() && undo.GetCount(), t_("Undo"), CtrlImg::undo(), USERBACK(Undo))
	   .Repeat()
	   .Key(K_ALT_BACKSPACE)
	   .Key(key);
}

void WPEditor::RedoTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly() && redo.GetCount(), t_("Redo"), CtrlImg::redo(), USERBACK(Redo))
	   .Repeat()
	   .Key(K_SHIFT|K_ALT_BACKSPACE)
	   .Key(key);
}

void WPEditor::CutTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly() && IsSelection() || objectpos >= 0, t_("Cut"), CtrlImg::cut(), USERBACK(Cut))
	   .Key(K_SHIFT_DELETE)
	   .Key(key);
}

void WPEditor::CopyTool(Bar& bar, dword key)
{
	bar.Add(IsSelection() || objectpos >= 0,
	        t_("Copy"), CtrlImg::copy(), USERBACK(Copy))
	   .Key(K_CTRL_INSERT)
	   .Key(key);
}

void WPEditor::PasteTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Paste"), CtrlImg::paste(), USERBACK(Paste))
	   .Key(K_SHIFT_INSERT)
	   .Key(key);
}

void WPEditor::ObjectTool(Bar& bar, dword key)
{
	for(int i = 0; i < RichObject::GetTypeCount(); i++) {
		String cn = RichObject::GetType(i).GetCreateName();
		if(!IsNull(cn))
			bar.Add(cn, USERBACK1(InsertObject, i));
	}
}

void WPEditor::LoadImageTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Insert image from file.."), WPEditCtrlImg::LoadImageFile(), THISBACK(InsertImage));
}

void WPEditor::PrintTool(Bar& bar, dword key)
{
	bar.Add(t_("Print"), CtrlImg::print(), USERBACK(DoPrint))
	   .Key(key);
}

void WPEditor::FindReplaceTool(Bar& bar, dword key)
{
	bar.Add(!IsReadOnly(), t_("Find/Replace"), WPEditCtrlImg::FindReplace(), USERBACK(OpenFindReplace))
	   .Key(key);
}

void WPEditor::EditTools(Bar& bar)
{
	CutTool(bar);
	CopyTool(bar);
	PasteTool(bar);
	bar.Gap();
	UndoTool(bar);
	RedoTool(bar);
	bar.Gap();
	FindReplaceTool(bar);
}

void WPEditor::InsertTableTool(Bar& bar, dword key)
{
	bar.Add(!IsSelection() && !IsReadOnly(),
	        t_("Insert table.."), WPEditCtrlImg::InsertTable(), USERBACK(InsertTable))
	   .Key(key);
}

void WPEditor::TablePropertiesTool(Bar& bar, dword key)
{
	bar.Add(!IsSelection() && cursorp.table && !IsReadOnly(),
	        t_("Table properties.."), WPEditCtrlImg::TableProperties(),
	        USERBACK(TableProps))
	   .Key(key);
}

void WPEditor::InsertTableRowTool(Bar& bar, dword key)
{
	bar.Add(!IsSelection() && cursorp.table && !IsReadOnly(),
	        t_("Insert row"), WPEditCtrlImg::TableInsertRow(),
	        USERBACK(TableInsertRow))
	   .Key(key);
}

void WPEditor::RemoveTableRowTool(Bar& bar, dword key)
{
	bar.Add(!IsSelection() && cursorp.table && !IsReadOnly(),
	        t_("Remove row"), WPEditCtrlImg::TableRemoveRow(), USERBACK(TableRemoveRow))
	   .Key(key);
}

void WPEditor::InsertTableColumnTool(Bar& bar, dword key)
{
	bar.Add(!IsSelection() && cursorp.table && !IsReadOnly(),
	        t_("Insert column"), WPEditCtrlImg::TableInsertColumn(),
	        USERBACK(TableInsertColumn))
	   .Key(key);
}

void WPEditor::RemoveTableColumnTool(Bar& bar, dword key)
{
	bar.Add(!IsSelection() && cursorp.table && !IsReadOnly(),
	        t_("Remove column"), WPEditCtrlImg::TableRemoveColumn(), USERBACK(TableRemoveColumn))
	   .Key(key);
}

void WPEditor::SplitJoinCellTool(Bar& bar, dword key)
{
	if(tablesel)
		bar.Add(!IsReadOnly(), t_("Join cells"), WPEditCtrlImg::JoinCell(), USERBACK(JoinCell))
		   .Key(key);
	else
		bar.Add(!IsSelection() && cursorp.table && !IsReadOnly(), t_("Split cell.."),
		        WPEditCtrlImg::SplitCell(), USERBACK(SplitCell))
		   .Key(key);
}

void WPEditor::CellPropertiesTool(Bar& bar, dword key)
{
	bar.Add(cursorp.table && (!IsSelection() || tablesel) && !IsReadOnly(),
	        t_("Cell properties.."), WPEditCtrlImg::CellProperties(), USERBACK(CellProperties))
	   .Key(key);
}

void WPEditor::TableTools(Bar& bar)
{
	InsertTableTool(bar);
	TablePropertiesTool(bar);
	InsertTableRowTool(bar);
	RemoveTableRowTool(bar);
	InsertTableColumnTool(bar);
	RemoveTableColumnTool(bar);
	SplitJoinCellTool(bar);
	CellPropertiesTool(bar);
}

void WPEditor::InsertImageTool(Bar& bar)
{
	bar.Add(t_("Insert image from file.."), USERBACK(InsertImage));
}

void WPEditor::StyleKeysTool(Bar& bar)
{
	bar.Add(t_("Style keys.."), USERBACK(StyleKeys));
}

void WPEditor::HeaderFooterTool(Bar& bar)
{
	bar.Add(t_("Header/Footer.."), WPEditCtrlImg::HeaderFooter(), USERBACK(HeaderFooter));
}

void WPEditor::DefaultBar(Bar& bar, bool extended)
{
	EditTools(bar);
	bar.Gap();
	PrintTool(bar);
	bar.Gap();
	FontTools(bar);
	bar.Gap();
	InkTool(bar);
	PaperTool(bar);
	bar.Gap();
	LanguageTool(bar);
	SpellCheckTool(bar);
	if(extended) {
		bar.Gap();
		IndexEntryTool(bar, INT_MAX);
	}
	bar.Break();
	StyleTool(bar);
	bar.Gap();
	ParaTools(bar);
	if(extended) {
		bar.Gap();
		HeaderFooterTool(bar);
	}
	bar.Gap();
	if(extended) {
		LabelTool(bar);
		bar.Gap();
	}
	TableTools(bar);
	if(extended) {
		bar.Gap();
		HyperlinkTool(bar, INT_MAX);
	}
}

//YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY


void WPEditor::SpellerAdd(const WString& w, int lang)
{
	Upp::SpellerAdd(w, lang);
}

int WPEditor::fixedlang;

Bits WPEditor::SpellParagraph(const RichPara& para)
{
	int len = para.GetLength();
	Buffer<wchar> text(len);
	Buffer<int> lang(len);
	wchar *s = text;
	int *g = lang;
	for(int i = 0; i < para.GetCount(); i++) {
		const RichPara::Part& p = para[i];
		if(p.IsText()) {
			int l = p.text.GetLength();
			memcpy(s, p.text, l * sizeof(wchar));
			Fill(g, g + l, fixedlang ? fixedlang : p.format.language);
			s += l;
			g += l;
		}
		else {
			*s++ = 127;
			*g++ = 0;
		}
	}
	Bits e;
	s = text;
	wchar *end = text + len;
	while(s < end) {
		if(IsLetter(*s)) {
			const wchar *q = s;
			while(s < end && IsLetter(*s) || s + 1 < end && *s == '\'' && IsLetter(s[1]))
				s++;
			if(!SpellWord(q, int(s - q), lang[q - text]))
				e.SetN(int(q - text), int(s - q));
		}
		else
			s++;
	}
	return e;
}

//ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ


void WPEditor::FinishNF()
{
	cursor = clamp(cursor, 0, text.GetLength());
	anchor = clamp(anchor, 0, text.GetLength());
	anchorp = text.GetRichPos(anchor);
	cursorp = text.GetRichPos(cursor);
	tablesel = 0;
	begtabsel = false;
	if(anchor != cursor) {
		RichPos p = text.GetRichPos(cursor, anchorp.level);
		if(anchorp.level == 0 || anchorp.level < cursorp.level) {
			cursor = text.AdjustCursor(anchor, cursor);
			cursorp = text.GetRichPos(cursor);
		}
		else
		if(p.table != anchorp.table) {
			if(anchor == 0 && anchorp.level == 1 && text.GetRichPos(anchor, 1).table == 1 && anchor < cursor) {
				while(cursor > 0 && cursorp.level) // selection must at at plain text
					cursorp = text.GetRichPos(--cursor);
				begtabsel = true;
				anchor = 0;
			}
			else {
				tablesel = anchorp.table;
				if(cursor < anchor) {
					cells.left = 0;
					cells.right = anchorp.cell.x;
					cells.top = 0;
					cells.bottom = anchorp.cell.y;
				}
				else {
					cells.left = anchorp.cell.x;
					cells.right = anchorp.tabsize.cx - 1;
					cells.top = anchorp.cell.y;
					cells.bottom = anchorp.tabsize.cy - 1;
				}
				text.AdjustTableSel(tablesel, cells);
			}
		}
		else
		if(p.cell != anchorp.cell) {
			tablesel = anchorp.table;
			cells.left = min(anchorp.cell.x, p.cell.x);
			cells.right = max(anchorp.cell.x, p.cell.x);
			cells.top = min(anchorp.cell.y, p.cell.y);
			cells.bottom = max(anchorp.cell.y, p.cell.y);
			text.AdjustTableSel(tablesel, cells);
		}
	}
	cursorc = text.GetCaret(cursor, pagesz);
	Size sz = GetSize();
	SetSb();
	Rect r = PlaceCaret();
	if(r.top == GetPosY(text.GetCaret(0, pagesz)))
		sb = 0;
	else
		sb.ScrollInto(r.top, r.Height());
	sb.ScrollInto(r.bottom, 1); // if r.Height is bigger than view height, make sure we rather see the bottom
	SetZsc();
	PageY top, bottom;
	int sell = min(cursor, anchor);
	int selh = max(cursor, anchor);
	if(tablesel)
		Refresh();
	else
	if(text.GetInvalid(top, bottom, pagesz, sell, selh, osell, oselh)) {
		int y = GetPosY(top);
		Refresh(0, y - sb, sz.cx, GetPosY(bottom) - y);
		y = GetPosY(text.GetHeight(pagesz)) - sb;
		if(y < sz.cy)
			Refresh(0, y, sz.cx, sz.cy - y);
	}
	osell = sell;
	oselh = selh;
	text.Validate();
	FixObjectRect();
	SetupRuler();
	if(modified) {
		if(useraction)
			Action();
	}
	useraction = modified = false;
}

void WPEditor::Finish()
{
	FinishNF();
	ReadFormat();
}

void WPEditor::MoveNG(int newpos, bool select)
{
	if(newpos < 0) newpos = 0;
	if(newpos >= text.GetLength() + select) newpos = text.GetLength() + select;
	CloseFindReplace();
	cursor = newpos;
	if(!select) {
		anchor = cursor;
		begtabsel = false;
	}
	objectpos = -1;
	Finish();
	if(select)
		SetSelectionSource(String().Cat() << "text/QTF;Rich Text Format;text/rtf;application/rtf;"
		                   << ClipFmtsText());
}

void WPEditor::Move(int newpos, bool select)
{
	MoveNG(newpos, select);
	gx = cursorc.left;
}

void WPEditor::MoveUpDown(int dir, bool select, int pg)
{
	Rect page = pagesz;
	if(dir > 0 && cursor >= GetLength() && select) {
		Move(GetLength() + 1, true);
		return;
	}
	if(dir < 0 && cursor > GetLength()) {
		Move(GetLength(), select);
		return;
	}
	int c = text.GetVertMove(min(GetLength(), cursor), gx, page, dir);
	if(c >= 0)
		MoveNG(c, select);
	else
		Move(dir < 0 ? 0 : GetLength(), select);
	if(pg) {
		RichCaret pr = text.GetCaret(cursor, pagesz);
		PageY py;
		py.page = pr.page;
		py.y = pr.top + dir * pg;
		while(py.y > pagesz.cy) {
			py.y -= pagesz.cy;
			py.page++;
		}
		while(py.y < 0) {
			py.y += pagesz.cy;
			py.page--;
		}
		MoveNG(text.GetPos(pr.left, py, pagesz), select);
	}
}

void WPEditor::MovePageUpDown(int dir, bool select)
{
	PageRect p = text.GetCaret(cursor, pagesz);
	int q = GetPosY(p) - sb;
	MoveUpDown(dir, select, 4 * GetTextRect().Height() / GetZoom() / 5);
	p = text.GetCaret(cursor, pagesz);
	sb = GetPosY(p) - q;
}

void WPEditor::MoveHomeEnd(int dir, bool select)
{
	int c = cursor;
	while(c + dir >= 0 && c + dir <= text.GetLength()) {
		PageRect p1 = text.GetCaret(c + dir, pagesz);
		if(p1.page != cursorc.page || p1.top != cursorc.top)
			break;
		c += dir;
	}
	Move(c, select);
}

bool WPEditor::IsW(int c)
{
	return IsLetter(c) || IsDigit(c) || c == '_';
}

void WPEditor::MoveWordRight(bool select)
{
	Move((int)GetNextWord(cursor), select);
}

void WPEditor::MoveWordLeft(bool select)
{
	Move((int)GetPrevWord(cursor), select);
}

bool WPEditor::SelBeg(bool select)
{
	if(IsSelection() && !select) {
		Move(min(cursor, anchor), false);
		return true;
	}
	return false;
}

bool WPEditor::SelEnd(bool select)
{
	if(IsSelection() && !select) {
		Move(max(cursor, anchor), false);
		return true;
	}
	return false;
}

void WPEditor::SelCell(int dx, int dy)
{
	Move(text.GetCellPos(tablesel, minmax(cursorp.cell.y + dy, 0, cursorp.tabsize.cy - 1),
	                               minmax(cursorp.cell.x + dx, 0, cursorp.tabsize.cx - 1)).pos, true);
}

bool WPEditor::CursorKey(dword key, int count)
{
	bool select = key & K_SHIFT;
	if(key == K_CTRL_ADD) {
		ZoomView(1);
		return true;
	}
	if(key == K_CTRL_SUBTRACT) {
		ZoomView(-1);
		return true;
	}
	if(select && tablesel)
		switch(key & ~K_SHIFT) {
		case K_LEFT:
			SelCell(-1, 0);
			break;
		case K_RIGHT:
			SelCell(1, 0);
			break;
		case K_UP:
			SelCell(0, -1);
			break;
		case K_DOWN:
			SelCell(0, 1);
			break;
		default:
			return false;
		}
	else {
		switch(key) {
		case K_CTRL_UP:
			sb.PrevLine();
			break;
		case K_CTRL_DOWN:
			sb.NextLine();
			break;
		default:
			switch(key & ~K_SHIFT) {
			case K_LEFT:
				if(!SelBeg(select))
					Move(cursor - 1, select);
				break;
			case K_RIGHT:
				if(!SelEnd(select))
					Move(cursor + 1, select);
				break;
			case K_UP:
				if(!SelBeg(select))
					MoveUpDown(-1, select);
				break;
			case K_DOWN:
				if(!SelEnd(select))
					MoveUpDown(1, select);
				break;
			case K_PAGEUP:
				if(!SelBeg(select))
					MovePageUpDown(-1, select);
				break;
			case K_PAGEDOWN:
				if(!SelEnd(select))
					MovePageUpDown(1, select);
				break;
			case K_END:
				MoveHomeEnd(1, select);
				break;
			case K_HOME:
				MoveHomeEnd(-1, select);
				break;
			case K_CTRL_LEFT:
				if(!SelBeg(select))
					MoveWordLeft(select);
				break;
			case K_CTRL_RIGHT:
				if(!SelEnd(select))
					MoveWordRight(select);
				break;
			case K_CTRL_HOME:
			case K_CTRL_PAGEUP:
				Move(0, select);
				break;
			case K_CTRL_END:
			case K_CTRL_PAGEDOWN:
				Move(text.GetLength(), select);
				break;
			case K_CTRL_A:
				Move(0, false);
				Move(text.GetLength(), true);
				break;
			default:
				return false;
			}
		}
	}
	Sync();
	return true;
}

bool WPEditor::IsSelection() const
{
	return anchor != cursor;
}

bool WPEditor::GetSelection(int& l, int& h) const
{
	if(IsSelection()) {
		l = min(anchor, cursor);
		h = max(anchor, cursor);
		return true;
	}
	l = h = cursor;
	return false;
}

bool WPEditor::InSelection(int& c) const
{
	int sell, selh;
	if(GetSelection(sell, selh) && c >= sell && c < selh) {
		c = sell;
		return true;
	}
	return false;
}

void WPEditor::CancelSelection()
{
	if(IsSelection()) {
		tablesel = 0;
		anchor = cursor;
		begtabsel = false;
		found = notfoundfw = false;
		CloseFindReplace();
		Finish();
	}
}

bool WPEditor::RemoveSelection(bool back)
{
	if(IsSelection()) {
		if(tablesel) {
			NextUndo();
			SaveTable(tablesel);
			text.ClearTable(tablesel, cells);
			Move(text.GetCellPos(tablesel, cells.top, cells.left).pos);
		}
		else {
			BegSelTabFix();
			int c = min(cursor, anchor);
			Remove(c, abs(cursor - anchor), back);
			found = notfoundfw = false;
			CloseFindReplace();
			Move(c);
		}
		return true;
	}
	return false;
}

WString WPEditor::GetWordAtCursor()
{
	WString w;
	int c = cursor;
	if(IsLetter(text[c])) {
		while(c > 0 && IsLetter(text[c - 1]))
			c--;
		while(w.GetLength() < 64 && IsLetter(text[c])) {
			w.Cat(text[c]);
			c++;
		}
	}
	return w;
}

void WPEditor::AddUserDict()
{
	if(IsSelection()) return;
	WString w = GetWordAtCursor();
	if(w.IsEmpty()) return;
	SpellerAdd(w, fixedlang ? fixedlang : formatinfo.language);
	text.ClearSpelling();
	Refresh();
}

void WPEditor::Goto()
{
	SetFocus();
	if(gototable.IsCursor())
	{
		Move(gototable.Get(1), false);
		Move(gototable.Get(2), true);
	}
}

void WPEditor::GotoType(int type, Ctrl& l)
{
	Vector<RichValPos> f = text.GetValPos(pagesz, type);
	gototable.Clear();
	for(int i = 0; i < f.GetCount(); i++) {
		const RichValPos& q = f[i];
		int endpos = q.pos;
		if(type == RichText::INDEXENTRIES) {
			WString ie = text.GetRichPos(endpos).format.indexentry;
			int l = text.GetLength();
			while(endpos < l) {
				RichPos p = text.GetRichPos(++endpos);
				if(p.format.indexentry != ie || p.chr == '\n')
					break;
			}
		}
		gototable.Add(q.data, q.pos, endpos);
	}
	if(gototable.GetCount())
		gototable.PopUp(&l);
}

void WPEditor::GotoLbl()
{
	GotoType(RichText::LABELS, label);
}

void WPEditor::GotoEntry()
{
	GotoType(RichText::INDEXENTRIES, indexentry);
}

bool WPEditor::GotoLabel(const String& lbl)
{
	Vector<RichValPos> f = text.GetValPos(pagesz, RichText::LABELS);
	for(int i = 0; i < f.GetCount(); i++)
		if(f[i].data == WString(lbl)) {
			Move(f[i].pos);
			return true;
		}
	return false;
}

void WPEditor::BeginPara()
{
	RichPos pos = text.GetRichPos(anchor);
	Move(cursor - pos.posinpara);
}

void WPEditor::NextPara()
{
	RichPos pos = text.GetRichPos(anchor);
	Move(cursor - pos.posinpara + pos.paralen + 1);
}

void WPEditor::PrevPara()
{
	RichPos pos = text.GetRichPos(anchor);
	Move(cursor - pos.posinpara - 1);
	BeginPara();
}

//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA


void WPHotPaint(Draw& w, int x, int y, const Image& img)
{
	Point p = img.GetHotSpot();
	w.DrawImage(x - p.x, y - p.y, img);
}

WPRuler::WPRuler()   { newtabalign = ALIGN_LEFT; }
WPRuler::~WPRuler()  {}

void WPRuler::FrameLayout(Rect& r)
{
	LayoutFrameTop(r, this, Arial(Zy(10)).GetHeight() + Zy(8));
}

void WPRuler::FrameAddSize(Size& sz)
{
	sz.cy += Arial(Zy(10)).GetHeight() + Zy(8);
}

void WPRuler::Paint(Draw& w)
{
	Size sz = GetSize();
	w.DrawRect(sz, SColorFace);
	w.DrawRect(0, sz.cy - Zx(1), sz.cx, Zy(1), SColorShadow);
	int cx = zoom * pgcx;
	w.DrawRect(x0 - Zx(1), Zy(3), cx + Zx(3), sz.cy - Zy(6), SColorPaper);
	int i = 0;
	for(;;) {
		int x = fround(++i * grid) * zoom;
		if(x >= cx) break;
		int h = (sz.cy - Zy(6)) / 3;
		if(i % marks == 0)
			w.DrawRect(x0 + x, Zy(2) + h, Zx(1), h + Zy(2), SColorHighlight);
		else
			w.DrawRect(x0 + x, Zy(3) + h, Zx(1), h, SColorHighlight);
	}
	i = 0;
	int xs = 0;
	for(;;)
		if(++i % numbers == 0) {
			int x = fround(i * grid) * zoom;
			if(x >= cx) break;
			String n = Format("%d", (int)(i * numbermul + 0.5));
			Size tsz = GetTextSize(n, ArialZ(9));
			int px = x0 + x - tsz.cx / 2;
			if(px >= xs && x + tsz.cx - tsz.cx / 2 < cx) {
				w.DrawRect(px, Zy(4), tsz.cx, sz.cy - Zy(8), SColorPaper);
				
				w.DrawText(px, Zy(4) + (sz.cy - Zy(8) - tsz.cy) / 2,
				           n, ArialZ(9), SColorText);
				xs = px + tsz.cx + Zx(4);
			}
		}
	FieldFrame().FramePaint(w, RectC(x0 - Zx(1), Zy(3), cx + Zx(3), sz.cy - Zy(6)));
	for(i = marker.GetCount() - 1; i >= 0; --i) {
		const Marker& m = marker[i];
		if(!IsNull(m.pos))
			WPHotPaint(w, x0 + m.pos * zoom, m.top ? Zy(1) : sz.cy - Zy(4), DPI(m.image));
	}
	i = 0;
	if(tabsize)
		for(;;) {
			int xp = ++i * tabsize;
			int x = xp * zoom;
			if(x >= cx) break;
			if(xp > tabpos)
				w.DrawRect(x0 + x, sz.cy - Zy(4), Zx(1), Zy(3), SColorShadow);
		}
	w.DrawImage(Zx(4), Zy(6), newtabalign == ALIGN_RIGHT  ? WPEditCtrlImg::RightTab() :
	                          newtabalign == ALIGN_CENTER ? WPEditCtrlImg::CenterTab() :
	                                                        WPEditCtrlImg::LeftTab());
}

int WPRuler::FindMarker(Point p)
{
	int x = p.x - x0;
	bool top = p.y < GetSize().cy / 2;
	for(int i = 0; i < marker.GetCount(); i++) {
		const Marker& m = marker[i];
		int hx = m.image.GetHotSpot().x;
		int cx = m.image.GetSize().cx;
		int p = zoom * m.pos;
		if(m.top == top && x >= p - hx && x < p + cx - hx)
			return i;
	}
	return -1;
}

void WPRuler::LeftDown(Point p, dword)
{
	track = FindMarker(p);
	if(track >= 0) {
		trackdx = marker[track].pos * zoom + x0 - p.x;
		SetCapture();
		WhenBeginTrack();
	}
	else
	if(p.x < Zx(16)) {
		newtabalign++;
		if(newtabalign > ALIGN_CENTER) newtabalign = ALIGN_LEFT;
		Refresh();
		return;
	}
	else {
		pos = ((p.x - x0) / zoom + snap / 2) / snap * snap;
		WhenLeftDown();
	}
}

void WPRuler::LeftDouble(Point p, dword)
{
	if(p.x < x0 - Zx(3)) {
		newtabalign++;
		if(newtabalign > ALIGN_CENTER) newtabalign = ALIGN_LEFT;
		Refresh();
		return;
	}

	WhenLeftDouble();
}

void WPRuler::RightDown(Point p, dword)
{
	if(p.x < x0 - Zx(3)) {
		newtabalign--;
		if(newtabalign < ALIGN_LEFT) newtabalign = ALIGN_CENTER;
		Refresh();
		return;
	}

	track = FindMarker(p);
	if(track < 0)
		pos = ((p.x - x0) / zoom + snap / 2) / snap * snap;
	WhenRightDown();
}

void WPRuler::LeftUp(Point p, dword)
{
	track = -1;
	WhenEndTrack();
}

void WPRuler::MouseMove(Point p, dword flags)
{

	if(HasCapture() && track >= 0) {
		Marker& m = marker[track];
		if((p.y < Zy(-10) || p.y >= GetSize().cy + Zy(10)) && m.deletable)
			m.pos = Null;
		else {
			int x = ((p.x + trackdx - x0) / zoom);
			if(!(flags & K_ALT))
				x = (x + snap / 2) / snap * snap;
			m.pos = minmax(x, m.minpos, m.maxpos);
		}
		Refresh();
		WhenTrack();
	}
}

void WPRuler::SetLayout(int x, int _pgcx, Zoom _zoom,
                          double _grid, int _numbers, double _numbermul, int _marks, int _snap)
{
	if(x0 != x || pgcx != _pgcx || zoom != _zoom || grid != _grid || numbers != _numbers ||
	   numbermul != _numbermul || marks != _marks || snap != _snap) {
		x0 = x;
		pgcx = _pgcx;
		zoom = _zoom;
		grid = _grid;
		numbers = _numbers;
		numbermul = _numbermul;
		marks = _marks;
		snap = _snap;
		Refresh();
	}
}

void WPRuler::Clear()
{
	if(marker.GetCount()) {
		marker.Clear();
		Refresh();
	}
}

void WPRuler::SetCount(int n)
{
	if(marker.GetCount() != n) {
		marker.SetCount(n);
		Refresh();
	}
}

void WPRuler::Set(int i, const Marker& m)
{
	if(i >= marker.GetCount() || marker[i] != m) {
		marker.At(i) = m;
		Refresh();
	}
}

void WPRuler::SetTabs(int pos, int size)
{
	if(tabpos != pos || tabsize != size) {
		tabpos = pos;
		tabsize = size;
		Refresh();
	}
}

//BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB


class StyleKeysDlg : public WithStyleKeysLayout<TopWindow> {
	typedef StyleKeysDlg CLASSNAME;
	
	Array<DropList>    style;
	Array<DropList>    face;
	Array<DropList>    height;
	Array<ColorPusher> ink;
	Array<ColorPusher> paper;

public:
	void Init(const WPEditor& edit, WPEditor::StyleKey *key);
	void Retrieve(WPEditor::StyleKey *k);

	StyleKeysDlg();
};

StyleKeysDlg::StyleKeysDlg()
{
	CtrlLayoutOKCancel(*this, "Styling keys");
	
	list.AddColumn("Key");
	list.AddColumn("Paragraph style");
	list.AddColumn("Font");
	list.AddColumn("Height");
	list.AddColumn("Ink");
	list.AddColumn("Paper");
	list.SetLineCy(EditField::GetStdHeight() + 4);
	list.NoHorzGrid().EvenRowColor().NoCursor();
	list.ColumnWidths("117 160 160 75 90 90");
}
	
void StyleKeysDlg::Init(const WPEditor& edit, WPEditor::StyleKey *key)
{
	const RichText& text = edit.text;
	for(int i = 0; i < 20; i++) {
		WPEditor::StyleKey& k = key[i];
		list.Add((i >= 10 ? "Shift+Alt+" : "Alt+") + AsString(i % 10));

		DropList& st = style.At(i);
		st.Add(Null);
		const RichStyles& ts = text.GetStyles();
		for(int j = 0; j < ts.GetCount(); j++) {
			st.Add(ts.GetKey(j), ts[j].name);
		}
		if(st.FindKey(k.styleid) < 0)
			st.Add(k.styleid, k.stylename);
		st <<= k.styleid;
		list.SetCtrl(i, 1, st, false);
		
		DropList& fc = face.At(i);
		fc.Add(Null);
		for(int j = 0; j < edit.face.GetCount(); j++)
			fc.Add(Font::GetFaceName(edit.face.GetKey(j)));
		if(fc.Find(k.face) < 0)
			fc.Add(k.face);
		fc <<= k.face;
		list.SetCtrl(i, 2, fc, false);

		DropList& hg = height.At(i);
		hg.Add(Null);
		for(int j = 0; j < edit.height.GetCount(); j++)
			hg.Add(edit.height.Get(j));
		if(hg.Find(k.face) < 0)
			hg.Add(k.face);
		list.SetCtrl(i, 3, hg, false);
		
		ColorPusher& n = ink.At(i);
		n.NullText("");
		n <<= k.ink;
		list.SetCtrl(i, 4, n, false);
		
		ColorPusher& p = paper.At(i);
		p.NullText("");
		p <<= k.paper;
		list.SetCtrl(i, 5, p, false);
	}
}

void StyleKeysDlg::Retrieve(WPEditor::StyleKey *key)
{
	for(int i = 0; i < 20; i++) {
		WPEditor::StyleKey& k = key[i];
		k.styleid = ~style[i];
		k.stylename = style[i].GetValue();
		k.face = ~face[i];
		k.height = ~height[i];
		k.ink = ~ink[i];
		k.paper = ~paper[i];
	}
}

void WPEditor::StyleKeys()
{
	StyleKeysDlg dlg;
	dlg.Init(*this, stylekey);
	if(dlg.Run() == IDOK)
		dlg.Retrieve(stylekey);
}

void WPEditor::ApplyStyleKey(int i)
{
	if(i < 0 || i >= 20)
		return;
	const StyleKey& k = stylekey[i];
	if(!IsNull(k.styleid)) {
		int q = style.FindKey(k.styleid);
		if(q >= 0) {
			style.SetIndex(q);
			Style();
		}
	}
	if(!IsNull(k.face)) {
		int q = face.FindValue(k.face);
		if(q >= 0) {
			face.SetIndex(q);
			SetFace();
		}
	}
	if(!IsNull(k.height)) {
		height <<= k.height;
		SetHeight();
	}
	if(!IsNull(k.ink)) {
		ink <<= k.ink;
		SetInk();
	}
	if(!IsNull(k.paper)) {
		paper <<= k.paper;
		SetPaper();
	}
}

WPEditor::StyleKey::StyleKey()
{
	styleid = Null;
	height = Null;
	ink = Null;
	paper = Null;
}

//CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC


struct ParaFormatDlg : public WithParaFormatLayout<TopWindow> {
	WPParaFormatting para;

	ParaFormatDlg() {
		CtrlLayoutOKCancel(*this, t_("Paragraph format"));
		ActiveFocus(para.before);
	}
};

void WPEditor::ParaFormat()
{
	ParaFormatDlg d;
	d.para.Set(unit, formatinfo, !IsSelection() && cursorp.level == 0);
	if(d.Execute() != IDOK || !d.para.IsChanged())
		return;
	dword v = d.para.Get(formatinfo);
	if(v) ApplyFormat(0, v);
}

struct sCompareLess {
	bool operator()(const Value& a, const Value& b) const {
		return CompareNoCase(String(a), String(b)) < 0;
	}
};

void WPEditor::ReadStyles()
{
	int i;
	style.Clear();
	Vector<Uuid> id;
	Vector<String> name;
	for(i = 0; i < text.GetStyleCount(); i++)
	{
		id.Add(text.GetStyleId(i));
		name.Add(text.GetStyle(i).name);
	}
	IndexSort(name, id, sCompareLess());
	for(i = 0; i < id.GetCount(); i++) style.Add(id[i], name[i]);
}

int WPEditor::CompareStyle(const Value& a, const Value& b)
{
	return CompareNoCase(String(a), String(b));
}

void WPEditor::SetStyle()
{
	if(!IsSelection()) {
		NextUndo();
		WithSetStyleLayout<TopWindow> d;
		CtrlLayoutOKCancel(d, t_("Set style"));
		d.newstyle <<= d.Breaker(IDYES);
		d.style.AddKey();
		d.style.AddColumn();
		d.style.NoHeader().NoGrid();
		for(int i = 0; i < text.GetStyleCount(); i++)
			d.style.Add(text.GetStyleId(i), text.GetStyle(i).name);
		d.style.Sort(1, CompareStyle);
		int q = d.style.Find(RichStyle::GetDefaultId());
		if(q >= 0)
			d.style.SetDisplay(q, 0, Single<DisplayDefault>());
		d.style.FindSetCursor(formatinfo.styleid);
		RichStyle cs;
		cs.format = formatinfo;
		cs.format.sscript = 0;
		cs.format.link.Clear();
		cs.format.indexentry.Clear();
		cs.format.language = LNG_ENGLISH;
		cs.format.label.Clear();

		Uuid id;
		switch(d.Run()) {
		case IDCANCEL:
			return;
		case IDOK:
			if(d.style.IsCursor()) {
				id = d.style.GetKey();
				const RichStyle& st = text.GetStyle(id);
				cs.name = st.name;
				cs.next = st.next;
				SaveStyleUndo(id);
				break;
			}
			return;
		case IDYES:
			String newname;
			if(EditText(newname, Format(t_("New style no. %d"), text.GetStyleCount()),
			            "Name", CharFilterAscii128)) {
				cs.name = newname;
				id = Uuid::Create();
				cs.next = id;
				SaveStylesUndo();
				break;
			}
			return;
		}
		text.SetStyle(id, cs);
		ReadStyles();
		formatinfo.styleid = id;
		SaveFormat(GetCursor(), 0);
		text.ReStyle(GetCursor(), id);
		Finish();
	}
}

void WPEditor::Styles()
{
	NextUndo();
	WPStyleManager s;
	s.Setup(ffs, unit);
	s.Set(text);
	if(s.Execute() != IDOK || !s.IsChanged())
		return;
	SaveStylesUndo();
	SetModify();
	s.Get(text);
	ReadStyles();
	Finish();
}

void WPEditor::ApplyStylesheet(const RichText& r)
{
	NextUndo();
	SaveStylesUndo();
	text.OverrideStyles(r.GetStyles(), false, false);
	ReadStyles();
	Finish();
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD


void WPEditor::ApplyFormat(dword charvalid, dword paravalid)
{
	if(IsReadOnly())
		return;
	RichText::FormatInfo f = formatinfo;
	f.charvalid = charvalid;
	f.paravalid = paravalid;
	if(objectpos >= 0) {
		ModifyFormat(objectpos, f, 1);
		Finish();
	}
	if(IsSelection()) {
		if(tablesel) {
			NextUndo();
			SaveTable(tablesel);
			text.ApplyTableFormatInfo(tablesel, cells, f);
		}
		else {
			int l = min(cursor, anchor);
			int h = max(cursor, anchor);
			RichPos rp = text.GetRichPos(h);
			if(rp.posinpara == 0 && h > l) {
				RichPos rp1 = text.GetRichPos(h - 1);
				if(InSameTxt(rp, rp1))
					h--;
			}
			ModifyFormat(l, f, h - l);
		}
		Finish();
	}
	else
	if(cursorp.paralen == 0) {
		ModifyFormat(cursor, f, 0);
		Finish();
	}
	else
	if(f.paravalid) {
		ModifyFormat(cursor, f, 0);
		Finish();
	}
	else
		RefreshBar();
}

void WPEditor::ApplyFormatInfo(const RichText::FormatInfo& fi)
{
	fi.ApplyTo(formatinfo);
	formatinfo.charvalid |= fi.charvalid;
	formatinfo.paravalid |= fi.paravalid;
	ApplyFormat(fi.charvalid, fi.paravalid);
}

void WPEditor::Bold()
{
	NextUndo();
	formatinfo.Bold(!(formatinfo.IsBold() && (formatinfo.charvalid & RichText::BOLD)));
	ApplyFormat(RichText::BOLD);
}

void WPEditor::Italic()
{
	NextUndo();
	formatinfo.Italic(!(formatinfo.IsItalic() && (formatinfo.charvalid & RichText::ITALIC)));
	ApplyFormat(RichText::ITALIC);
}

void WPEditor::Underline()
{
	NextUndo();
	formatinfo.Underline(!(formatinfo.IsUnderline() && (formatinfo.charvalid & RichText::UNDERLINE)));
	ApplyFormat(RichText::UNDERLINE);
}

void WPEditor::Strikeout()
{
	NextUndo();
	formatinfo.Strikeout(!(formatinfo.IsStrikeout() && (formatinfo.charvalid & RichText::STRIKEOUT)));
	ApplyFormat(RichText::STRIKEOUT);
}

void WPEditor::Capitals()
{
	NextUndo();
	formatinfo.capitals = !formatinfo.capitals && (formatinfo.charvalid & RichText::CAPITALS);
	ApplyFormat(RichText::CAPITALS);
}

void WPEditor::SetScript(int i)
{
	NextUndo();
	formatinfo.sscript = i;
	ApplyFormat(RichText::SSCRIPT);
}

void WPEditor::SetFace()
{
	NextUndo();
	formatinfo.Face(~face);
	ApplyFormat(RichText::FACE);
	SetFocus();
}

void WPEditor::SetHeight()
{
	NextUndo();
	formatinfo.Height(PtToDot(~height));
	ApplyFormat(RichText::HEIGHT);
	SetFocus();
}

void WPEditor::SetInk()
{
	NextUndo();
	formatinfo.ink = ~ink;
	ApplyFormat(RichText::INK);
	SetFocus();
}

void WPEditor::SetPaper()
{
	NextUndo();
	formatinfo.paper = ~paper;
	ApplyFormat(RichText::PAPER);
	SetFocus();
}

void WPEditor::SetLanguage()
{
	NextUndo();
	formatinfo.language = (int)~language;
	ApplyFormat(RichText::LANGUAGE);
	SetFocus();
}

void WPEditor::Language()
{
	WithRichLanguageLayout<TopWindow> d;
	CtrlLayoutOKCancel(d, t_("Language"));
	d.lang <<= ~language;
	if(d.Run() != IDOK)
		return;
	formatinfo.language = (int)~d.lang;
	ApplyFormat(RichText::LANGUAGE);
	SetFocus();
	if(!language.HasKey((int)~d.lang)) {
		Vector<int> h;
		for(int i = 0; i < language.GetCount(); i++)
			h.Add(language.GetKey(i));
		h.Add(~d.lang);
		SetupLanguage(pick(h));
	}
}

void WPEditor::IndentMark()
{
	WPRuler::Marker m;
	int l = formatinfo.lm;
	int r = cursorc.textpage.Width() - formatinfo.rm;
	m.pos = l + formatinfo.indent;
	m.minpos = max(l, 0);
	m.maxpos = max(r - 120, 0);
	m.top = true;
	m.image = formatinfo.paravalid & RichText::INDENT ? WPEditCtrlImg::Indent()
	                                                  : WPEditCtrlImg::IndentMixed();
	ruler.Set(2, m);
}

void WPEditor::ReadFormat()
{
	if(objectpos >= 0)
		formatinfo = text.GetFormatInfo(objectpos, 1);
	else
	if(IsSelection())
		if(tablesel)
			formatinfo = text.GetTableFormatInfo(tablesel, cells);
		else
			formatinfo = text.GetFormatInfo(min(cursor, anchor), abs(cursor - anchor));
	else {
		RichPos p = cursorp;
		if(cursor && p.posinpara)
			p = text.GetRichPos(cursor - 1);
		formatinfo.Set(p.format);
	}
	ShowFormat();
}

void WPEditor::ShowFormat()
{
	RefreshBar();

	if(formatinfo.charvalid & RichText::FACE)
		face <<= formatinfo.GetFace();
	else
		face <<= Null;

	if(formatinfo.charvalid & RichText::HEIGHT)
		height <<= DotToPt(formatinfo.GetHeight());
	else
		height <<= Null;

	if(formatinfo.charvalid & RichText::LINK)
		hyperlink <<= formatinfo.link;
	else
		hyperlink <<= Null;

	if(formatinfo.charvalid & RichText::INDEXENTRY)
		indexentry <<= formatinfo.indexentry;
	else
		indexentry <<= Null;

	if(formatinfo.charvalid & RichText::INK)
		ink <<= formatinfo.ink;
	else
		ink <<= Null;

	if(formatinfo.charvalid & RichText::PAPER)
		paper <<= formatinfo.paper;
	else
		paper <<= Null;

	if(formatinfo.charvalid & RichText::LANG)
		language <<= (int)formatinfo.language;
	else
		language <<= Null;

	if(IsSelection())
		label <<= Null;
	else
		label <<= formatinfo.label;

	int l = formatinfo.lm;
	int r = cursorc.textpage.Width() - formatinfo.rm;

	WPRuler::Marker m;
	m.pos = l;
	m.minpos = 0;
	m.maxpos = max(r - formatinfo.indent - 120, 0);
	m.image = formatinfo.paravalid & RichText::LM ? WPEditCtrlImg::Margin() : WPEditCtrlImg::MarginMixed();
	ruler.Set(0, m);

	m.pos = r;
	m.minpos = max(l + formatinfo.indent + 120, 0);
	m.maxpos = cursorc.textpage.Width();
	m.image = formatinfo.paravalid & RichText::RM ? WPEditCtrlImg::Margin() : WPEditCtrlImg::MarginMixed();
	ruler.Set(1, m);
	IndentMark();

	int maxpos = 0;
	m.minpos = 0;
	m.deletable = true;
	if(formatinfo.paravalid & RichText::TABS) {
		for(int i = 0; i < formatinfo.tab.GetCount(); i++) {
			RichPara::Tab tab = formatinfo.tab[i];
			m.pos = tab.pos;
			if(tab.pos > maxpos)
				maxpos = tab.pos;
			switch(tab.align) {
			case ALIGN_LEFT:
				m.image = WPEditCtrlImg::LeftTab();
				break;
			case ALIGN_RIGHT:
				m.image = WPEditCtrlImg::RightTab();
				break;
			case ALIGN_CENTER:
				m.image = WPEditCtrlImg::CenterTab();
				break;
			}
			ruler.Set(i + 3, m);
		}
		ruler.SetTabs(maxpos, formatinfo.tabsize);
		ruler.SetCount(formatinfo.tab.GetCount() + 3);
	}
	else {
		ruler.SetTabs(INT_MAX / 2, 1);
		ruler.SetCount(3);
	}

	if(formatinfo.paravalid & RichText::STYLE)
		style <<= formatinfo.styleid;
	else
		style <<= Null;
	setstyle->Enable(!IsSelection());
}

void WPEditor::HighLightTab(int r)
{
	WPRuler::Marker m = ruler[r + 3];
	RichPara::Tab tab = formatinfo.tab[r];
	m.image = tab.align == ALIGN_RIGHT  ? WPEditCtrlImg::RightTabTrack() :
	          tab.align == ALIGN_CENTER ? WPEditCtrlImg::CenterTabTrack() :
	                                      WPEditCtrlImg::LeftTabTrack();
	ruler.Set(r + 3, m);
}

void WPEditor::Hyperlink()
{
	String s = formatinfo.link;
	if(!IsSelection() && !IsNull(s) && cursorp.format.link == s && text[cursor] != '\n') {
		int l = cursor - 1;
		while(l >= 0 && text[l] != '\n' && text.GetRichPos(l).format.link == s)
			l--;
		l++;
		int h = cursor;
		while(h < text.GetLength() && text[h] != '\n' && text.GetRichPos(h).format.link == s)
			h++;
		if(l < h)
			Select(l, h - l);
	}
	WString linktext;
	WhenHyperlink(s, linktext);
	if(s != formatinfo.link || linktext.GetLength()) {
		formatinfo.link = s;
		hyperlink <<= s;
		NextUndo();
		ApplyFormat(RichText::LINK);
		if(linktext.GetLength()) {
			RemoveSelection();
			RichPara p;
			p.format = formatinfo;
			p.Cat(linktext, formatinfo);
			RichText txt;
			txt.SetStyles(text.GetStyles());
			txt.Cat(p);
			Insert(cursor, txt, true);
			Move(cursor + linktext.GetCount(), false);
		}
	}
	SetFocus();
}

void WPEditor::Label()
{
	if(IsSelection()) return;
	String s = formatinfo.label;
	WhenLabel(s);
	if(s != formatinfo.label) {
		formatinfo.label = s;
		NextUndo();
		ApplyFormat(0, RichText::LABEL);
		SetFocus();
	}
}

void WPEditor::IndexEntry()
{
	String s = formatinfo.indexentry.ToString();
	String s0 = s;
	WhenIndexEntry(s);
	if(s != s0) {
		formatinfo.indexentry = s.ToWString();
		ApplyFormat(RichText::INDEXENTRY);
		NextUndo();
		SetFocus();
	}
}

void WPEditor::BeginRulerTrack()
{
	NextUndo();
	SaveFormat();
	int r = ruler.GetTrack();
	if(r < 0) return;
	WPRuler::Marker m = ruler[r];
	switch(r) {
	case 0:
	case 1:
		m.image = WPEditCtrlImg::MarginTrack();
		break;
	case 2:
		m.image = WPEditCtrlImg::IndentTrack();
		break;
	default:
		HighLightTab(r - 3);
		return;
	}
	ruler.Set(r, m);
}

void WPEditor::SetParaFormat(dword paravalid)
{
	RichText::FormatInfo f = formatinfo;
	f.charvalid = 0;
	f.paravalid = paravalid;
	if(IsSelection())
		if(tablesel)
			text.ApplyTableFormatInfo(tablesel, cells, f);
		else
			text.ApplyFormatInfo(min(cursor, anchor), f, abs(cursor - anchor));
	else
		text.ApplyFormatInfo(cursor, f, 0);
}

void WPEditor::RulerTrack()
{
	int r = ruler.GetTrack();
	if(r < 0) return;
	WPRuler::Marker m = ruler[r];
	switch(r) {
	case 0:
		formatinfo.lm = m.pos;
		SetParaFormat(RichText::LM);
		IndentMark();
		break;
	case 1:
		formatinfo.rm = cursorc.textpage.Width() - m.pos;
		SetParaFormat(RichText::RM);
		break;
	case 2:
		formatinfo.indent = m.pos - formatinfo.lm;
		SetParaFormat(RichText::INDENT);
		break;
	default:
		formatinfo.tab[r - 3].pos = m.pos;
		SetParaFormat(RichText::TABS);
		int maxpos = 0;
		for(int i = 0; i < formatinfo.tab.GetCount(); i++) {
			RichPara::Tab tab = formatinfo.tab[i];
			if(tab.pos > maxpos)
				maxpos = tab.pos;
		}
		ruler.SetTabs(maxpos, formatinfo.tabsize);
		break;
	}
	FinishNF();
}

void WPEditor::TabAdd(int align)
{
	RichPara::Tab tab;
	tab.pos = ruler.GetPos();
	tab.align = align;
	if(formatinfo.tab.GetCount() > 30000 || tab.pos < 0 || tab.pos >= cursorc.textpage.Width()) return;
	formatinfo.tab.Add(tab);
	SetParaFormat(RichText::TABS);
	Finish();
}

void WPEditor::AddTab()
{
	NextUndo();
	SaveFormat();
	TabAdd(ruler.GetNewTabAlign());
}

void WPEditor::TabMenu()
{
	NextUndo();
	int r = ruler.GetTrack() - 3;
	if(r >= 0)
		HighLightTab(r);
	CallbackArgTarget<int> align;
	CallbackArgTarget<int> fill;
	MenuBar menu;
	menu.Add(t_("Left"), WPEditCtrlImg::LeftTab(), align[ALIGN_LEFT]);
	menu.Add(t_("Right"), WPEditCtrlImg::RightTab(), align[ALIGN_RIGHT]);
	menu.Add(t_("Center"), WPEditCtrlImg::CenterTab(), align[ALIGN_CENTER]);
	if(r >= 0) {
		int f = formatinfo.tab[r].fillchar;
		menu.Separator();
		menu.Add(t_("No fill"), fill[0])
		    .Radio(f == 0);
		menu.Add(t_("Fill with ...."), fill[1])
		    .Radio(f == 1);
		menu.Add(t_("Fill with ----"), fill[2])
		    .Radio(f == 2);
		menu.Add(t_("Fill with __"), fill[3])
		    .Radio(f == 3);
		menu.Separator();
		menu.Add(t_("Remove"), fill[-1]);
	}
	menu.Execute();
	if(!IsNull(align)) {
		SaveFormat();
		if(r >= 0) {
			formatinfo.tab[r].align = (int)align;
			SetParaFormat(RichText::TABS);
		}
		else
			TabAdd(align);
	}
	if(!IsNull(fill) && r >= 0) {
		SaveFormat();
		if(r >= 0) {
			if(fill == -1)
				formatinfo.tab[r].pos = Null;
			else
				formatinfo.tab[r].fillchar = (int)fill;
			SetParaFormat(RichText::TABS);
		}
	}
	Finish();
}

void WPEditor::AlignLeft()
{
	NextUndo();
	formatinfo.align = ALIGN_LEFT;
	ApplyFormat(0, RichText::ALIGN);
}

void WPEditor::AlignRight()
{
	NextUndo();
	formatinfo.align = ALIGN_RIGHT;
	ApplyFormat(0, RichText::ALIGN);
}

void WPEditor::AlignCenter()
{
	NextUndo();
	formatinfo.align = ALIGN_CENTER;
	ApplyFormat(0, RichText::ALIGN);
}

void WPEditor::AlignJustify()
{
	NextUndo();
	formatinfo.align = ALIGN_JUSTIFY;
	ApplyFormat(0, RichText::ALIGN);
}

void  WPEditor::SetBullet(int bullet)
{
	NextUndo();
	if((formatinfo.paravalid & RichText::BULLET) && formatinfo.bullet == bullet) {
		formatinfo.bullet = RichPara::BULLET_NONE;
		formatinfo.indent = formatinfo.paravalid & RichText::STYLE ?
		                       text.GetStyle(formatinfo.styleid).format.indent : 0;
	}
	else {
		formatinfo.bullet = bullet;
		formatinfo.indent = bullet_indent;
	}
	ApplyFormat(0, RichText::INDENT|RichText::BULLET);
}

void WPEditor::Style()
{
	NextUndo();
	SaveFormat(cursor, 0);
	formatinfo.Set(text.GetStyle((Uuid)~style).format);
	ApplyFormat(0, RichText::STYLE);
	SetFocus();
	Finish();
}

void WPEditor::AdjustObjectSize()
{
	NextUndo();
	RichObject obj = cursorp.object;
	if(!obj) return;
	WithObjectSizeLayout<TopWindow> d;
	CtrlLayoutOKCancel(d, t_("Object position"));
	Size sz = obj.GetSize();
	Size psz = GetPhysicalSize(obj);
	if(psz.cx == 0) psz.cx = 2000;
	if(psz.cy == 0) psz.cy = 2000;
	d.width.Set(unit, sz.cx);
	d.height.Set(unit, sz.cy);
	d.widthp.SetInc(5).Pattern("%.1f");
	d.widthp <<= 100.0 * sz.cx / psz.cx;
	d.heightp.SetInc(5).Pattern("%.1f");
	d.heightp <<= 100.0 * sz.cy / psz.cy;
	d.keepratio = obj.IsKeepRatio();
	d.width <<= d.height <<= d.widthp <<= d.heightp <<= d.Breaker(IDYES);
	d.ydelta.WithSgn().Set(unit, obj.GetYDelta());
	d.keepratio <<= d.Breaker(IDNO);
	for(;;) {
		switch(d.Run()) {
		case IDCANCEL:
			return;
		case IDYES:
			if(d.width.HasFocus() && !IsNull(d.width)) {
				d.widthp <<= 100 * (double)~d.width / psz.cx;
				if(d.keepratio) {
					d.height <<= psz.cy * (double)~d.width / psz.cx;
					d.heightp <<= ~d.widthp;
				}
			}
			if(d.height.HasFocus() && !IsNull(d.height)) {
				d.heightp <<= 100 * (double)~d.height / psz.cy;
				if(d.keepratio) {
					d.width <<= psz.cx * (double)~d.height / psz.cy;
					d.widthp <<= ~d.heightp;
				}
			}
			if(d.widthp.HasFocus() && !IsNull(d.widthp)) {
				d.width <<= psz.cx * (double)~d.widthp / 100;
				if(d.keepratio) {
					d.height <<= psz.cy * (double)~d.width / psz.cx;
					d.heightp <<= ~d.widthp;
				}
			}
			if(d.heightp.HasFocus() && !IsNull(d.heightp)) {
				d.height <<= psz.cy * (double)~d.heightp / 100;
				if(d.keepratio) {
					d.width <<= psz.cx * (double)~d.height / psz.cy;
					d.widthp <<= ~d.heightp;
				}
			}
			break;
		case IDNO:
			if(d.keepratio && !IsNull(d.width)) {
				d.widthp <<= 100 * (double)~d.width / psz.cx;
				if(d.keepratio) {
					d.height  <<= psz.cy * (double)~d.width / psz.cx;
					d.heightp <<= ~d.widthp;
				}
			}
			break;
		case IDOK:
			if(!IsNull(d.width) && (int)~d.width > 0)
				sz.cx = ~d.width;
			if(!IsNull(d.height) && (int)~d.height > 0)
				sz.cy = ~d.height;
			obj.SetSize(sz);
			if(!IsNull(d.ydelta))
				obj.SetYDelta(~d.ydelta);
			obj.KeepRatio(d.keepratio);
			ReplaceObject(obj);
			return;
		}
	}
}

//EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE


Size WPEditor::GetPhysicalSize(const RichObject& obj)
{
	if(ignore_physical_size)
		return 600 * obj.GetPixelSize() / 96;
	return obj.GetPhysicalSize();
}

void WPEditor::CancelMode()
{
	tabmove.table = 0;
	selclick = false;
	dropcaret.Clear();
}

void WPEditor::MouseWheel(Point p, int zdelta, dword keyflags)
{
	if(keyflags == K_CTRL) {
		if(IsNull(floating_zoom))
			ZoomView(sgn(zdelta));
		else {
			floating_zoom = minmax(floating_zoom + zdelta / 480.0, 0.5, 10.0);
			RefreshLayoutDeep();
		}
	}
	else
		sb.Wheel(zdelta);
}

RichHotPos WPEditor::GetHotPos(Point p)
{
	int x;
	PageY py;
	GetPagePoint(p, py, x);
	return text.GetHotPos(x, py, 4 / GetZoom(), pagesz);
}

void WPEditor::SetObjectPercent(int p)
{
	if(objectpos >= 0) {
		RichObject obj = GetObject();
		Size sz = GetPhysicalSize(obj) * p / 100;
		if(sz.cx > 0 && sz.cy > 0) {
			obj.SetSize(sz);
			obj.KeepRatio(true);
			ReplaceObject(obj);
		}
	}
}

void WPEditor::SetObjectYDelta(int pt)
{
	if(objectpos >= 0) {
		RichObject obj = GetObject();
		obj.SetYDelta(pt * 25 / 3);
		ReplaceObject(obj);
	}
}

void WPEditor::SetObjectPos(int pos)
{
	Rect r = GetObjectRect(cursor);
	Rect rr = r.Offseted(GetTextRect().left, -sb);
	objectrect = GetObjectRect(pos);
	objectpos = cursor;
	PlaceCaret();
	Refresh(rr);
	ReadFormat();
}

void WPEditor::LeftDown(Point p, dword flags)
{
	useraction = true;
	NextUndo();
	SetFocus();
	selclick = false;
	tabmove = GetHotPos(p);
	if(tabmove.table && tabmove.column >= -2) {
		SaveTableFormat(tabmove.table);
		SetCapture();
		Move(text.GetCellPos(tabmove.table, 0, max(tabmove.column, 0)).pos);
		return;
	}
	int c = GetHotSpot(p);
	if(c >= 0 && objectpos >= 0) {
		int pos = objectpos;
		RectTracker tracker(*this);
		RichObject obj = text.GetRichPos(pos).object;
		tracker.MinSize(Size(16, 16))
		       .MaxSize(GetZoom() * pagesz)
		       .Animation()
		       .Dashed()
		       .KeepRatio(obj.IsKeepRatio());
		int tx, ty;
		switch(c) {
		case 1:
			tracker.SetCursorImage(Image::SizeVert());
			tx = ALIGN_CENTER; ty = ALIGN_BOTTOM;
			break;
		case 2:
			tracker.SetCursorImage(Image::SizeHorz());
			tx = ALIGN_RIGHT; ty = ALIGN_CENTER;
			break;
		default:
			tracker.SetCursorImage(Image::SizeBottomRight());
			tx = ALIGN_RIGHT; ty = ALIGN_RIGHT;
			break;
		}
		double zoom = GetZoom().AsDouble();
		Size sz = obj.GetSize();
		sz.cx = int(zoom * sz.cx + 0.5);
		sz.cy = int(zoom * sz.cy + 0.5);
		sz = tracker.Track(Rect(objectrect.Offseted(GetTextRect().left, -sb).TopLeft(), sz), tx, ty).Size();
		sz.cx = int(sz.cx / zoom + 0.5);
		sz.cy = int(sz.cy / zoom + 0.5);
		obj.SetSize(sz);
		ReplaceObject(obj);
	}
	else {
		c = GetMousePos(p);
		if(c >= 0) {
			if(InSelection(c)) {
				selclick = true;
				return;
			}
			Move(c, flags & K_SHIFT);
			mpos = c;
			SetCapture();
			if(cursorp.object && GetObjectRect(cursor).Offseted(GetTextRect().left, -sb).Contains(p))
				SetObjectPos(cursor);
		}
	}
}

void WPEditor::LeftUp(Point p, dword flags)
{
	useraction = true;
	NextUndo();
	int c = GetMousePos(p);
	int d = c;
	if(!HasCapture() && InSelection(d) && selclick) {
		CancelSelection();
		Move(c);
	}
	selclick = false;
}

void WPEditor::MouseMove(Point p, dword flags)
{
	useraction = true;
	if(HasCapture() && (flags & K_MOUSELEFT)) {
		if(tabmove.table && tabmove.column >= 0) {
			RichTable::Format fmt = text.GetTableFormat(tabmove.table);
			if(tabmove.column >= fmt.column.GetCount() - 1)
				return;
			int sum = Sum(fmt.column);
			int nl = 0;
			for(int i = 0; i < tabmove.column; i++)
				nl += fmt.column[i];
			int xn = fmt.column[tabmove.column] + fmt.column[tabmove.column + 1];
			int xl = tabmove.left + tabmove.cx * nl / sum + 12;
			int xh = tabmove.left + tabmove.cx * (nl + xn) / sum - 12;
			if(xl >= xh)
				return;
			int xx = minmax(GetSnapX(p.x) - tabmove.delta, xl, xh) - tabmove.left;
			fmt.column[tabmove.column] = xx * sum / tabmove.cx - nl;
			fmt.column[tabmove.column + 1] = xn - fmt.column[tabmove.column];
			text.SetTableFormat(tabmove.table, fmt);
			Finish();
		}
		else
		if(tabmove.table && tabmove.column == RICHHOT_LM) {
			RichTable::Format fmt = text.GetTableFormat(tabmove.table);
			fmt.rm = clamp(fmt.rm, 0, tabmove.textcx - fmt.lm - 120);
			fmt.lm = clamp(GetSnapX(p.x) - tabmove.textleft, 0, max(tabmove.textcx - fmt.rm - 120, 0));
			text.SetTableFormat(tabmove.table, fmt);
			Finish();
		}
		else
		if(tabmove.table && tabmove.column == RICHHOT_RM) {
			RichTable::Format fmt = text.GetTableFormat(tabmove.table);
			fmt.lm = clamp(fmt.lm, 0, max(tabmove.textcx - fmt.rm - 120, 0));
			fmt.rm = minmax(tabmove.textcx - GetSnapX(p.x) + tabmove.textleft, 0, tabmove.textcx - fmt.lm - 120);
			text.SetTableFormat(tabmove.table, fmt);
			Finish();
		}
		else {
			PageY py = GetPageY(p.y + sb);
			int c;
			if(py > text.GetHeight(pagesz))
				c = GetLength() + 1;
			else
				c = GetNearestPos(GetX(p.x), py);
			if(c != mpos) {
				mpos = -1;
				Move(c, true);
			}
		}
	}
}

static bool IsObjectPercent(Sizef percent, int p)
{
	return fabs(percent.cx - p) < 1 && fabs(percent.cy - p) < 1;
}

static bool IsObjectDelta(int delta, int d)
{
	return d * 25 / 3 == delta;
}

void WPEditor::StdBar(Bar& menu)
{
	int l, h;
	Id field;
	String fieldparam;
	String ofieldparam;
	RichObject object;
	if(GetSelection(l, h)) {
		CutTool(menu);
		CopyTool(menu);
		PasteTool(menu);
	}
	else {
		if(objectpos >= 0) {
			bar_object = GetObject();
			if(!bar_object) return;
			bar_object.Menu(menu, context);
			if(!menu.IsEmpty())
				menu.Separator();
			Size sz = GetPhysicalSize(bar_object);
			Sizef percent = bar_object.GetSize();
			percent = 100.0 * percent / Sizef(sz);
			bool b = sz.cx || sz.cy;
			menu.Add(t_("Object position.."), THISBACK(AdjustObjectSize));
			menu.Separator();
			menu.Add(b, "20 %", THISBACK1(SetObjectPercent, 20)).Check(IsObjectPercent(percent, 20));
			menu.Add(b, "30 %", THISBACK1(SetObjectPercent, 30)).Check(IsObjectPercent(percent, 30));
			menu.Add(b, "40 %", THISBACK1(SetObjectPercent, 40)).Check(IsObjectPercent(percent, 40));
			menu.Add(b, "50 %", THISBACK1(SetObjectPercent, 50)).Check(IsObjectPercent(percent, 50));
			menu.Add(b, "60 %", THISBACK1(SetObjectPercent, 60)).Check(IsObjectPercent(percent, 60));
			menu.Add(b, "70 %", THISBACK1(SetObjectPercent, 70)).Check(IsObjectPercent(percent, 70));
			menu.Add(b, "80 %", THISBACK1(SetObjectPercent, 80)).Check(IsObjectPercent(percent, 80));
			menu.Add(b, "90 %", THISBACK1(SetObjectPercent, 90)).Check(IsObjectPercent(percent, 90));
			menu.Add(b, "100 %", THISBACK1(SetObjectPercent, 100)).Check(IsObjectPercent(percent, 100));
			menu.Break();
			int delta = bar_object.GetYDelta();
			menu.Add(t_("3 pt up"), THISBACK1(SetObjectYDelta, -3)).Check(IsObjectDelta(delta, -3));
			menu.Add(t_("2 pt up"), THISBACK1(SetObjectYDelta, -2)).Check(IsObjectDelta(delta, -2));
			menu.Add(t_("1 pt up"), THISBACK1(SetObjectYDelta, -1)).Check(IsObjectDelta(delta, -1));
			menu.Add(t_("Baseline"), THISBACK1(SetObjectYDelta, 0)).Check(IsObjectDelta(delta, 0));
			menu.Add(t_("1 pt down"), THISBACK1(SetObjectYDelta, 1)).Check(IsObjectDelta(delta, 1));
			menu.Add(t_("2 pt down"), THISBACK1(SetObjectYDelta, 2)).Check(IsObjectDelta(delta, 2));
			menu.Add(t_("3 pt down"), THISBACK1(SetObjectYDelta, 3)).Check(IsObjectDelta(delta, 3));
			menu.Separator();
			CopyTool(menu);
			CutTool(menu);
		}
		else {
			RichPos p = cursorp;
			field = p.field;
			bar_fieldparam = p.fieldparam;
			RichPara::FieldType *ft = RichPara::fieldtype().Get(field, NULL);
			if(ft) {
				ft->Menu(menu, &bar_fieldparam);
				if(!menu.IsEmpty())
					menu.Separator();
				CopyTool(menu);
				CutTool(menu);
			}
			else {
				WString w = GetWordAtCursor();
				if(!w.IsEmpty() && !SpellWord(w, w.GetLength(),
				                              fixedlang ? fixedlang : formatinfo.language)) {
					menu.Add(t_("Add to user dictionary"), THISBACK(AddUserDict));
					menu.Separator();
				}
				PasteTool(menu);
				ObjectTool(menu);
			}
		}
		LoadImageTool(menu);
	}
}

void WPEditor::RightDown(Point p, dword flags)
{
	useraction = true;
	NextUndo();
	MenuBar menu;
	int l, h;
	Rect ocr = GetCaretRect();
	int fieldpos = -1;
	Id field;
	String ofieldparam;
	RichObject o;
	bar_object.Clear();
	bar_fieldparam = Null;
	if(!GetSelection(l, h)) {
		LeftDown(p, flags);
		if(objectpos >= 0)
			o = bar_object = GetObject();
		else {
			RichPos p = cursorp;
			field = p.field;
			bar_fieldparam = p.fieldparam;
			RichPara::FieldType *ft = RichPara::fieldtype().Get(field, NULL);
			if(ft) {
				ofieldparam = bar_fieldparam;
				fieldpos = cursor;
			}
		}
	}
	WhenBar(menu);
	Rect r = GetCaretRect();
	Refresh(r);
	Refresh(ocr);
	paintcarect = true;
	menu.Execute();
	paintcarect = false;
	Refresh(r);
	if(bar_object && o && o.GetSerialId() != bar_object.GetSerialId())
		ReplaceObject(bar_object);
	if(fieldpos >= 0 && bar_fieldparam != ofieldparam) {
		RichText::FormatInfo f = text.GetFormatInfo(fieldpos, 1);
		Remove(fieldpos, 1);
		RichPara p;
		p.Cat(field, bar_fieldparam, f);
		RichText clip;
		clip.Cat(p);
		Insert(fieldpos, clip, false);
		Finish();
	}
	bar_object.Clear();
	bar_fieldparam = Null;
}

void WPEditor::LeftDouble(Point p, dword flags)
{
	NextUndo();
	int c = GetMousePos(p);
	if(c >= 0) {
		if(objectpos == c) {
			RichObject object = GetObject();
			if(!object) return;
			RichObject o = object;
			o.DefaultAction(context);
			if(object.GetSerialId() != o.GetSerialId())
				ReplaceObject(o);
		}
		else {
			RichPos rp = cursorp;
			RichPara::FieldType *ft = RichPara::fieldtype().Get(rp.field, NULL);
			if(ft) {
				int fieldpos = cursor;
				ft->DefaultAction(&rp.fieldparam);
				RichText::FormatInfo f = text.GetFormatInfo(fieldpos, 1);
				Remove(fieldpos, 1);
				RichPara p;
				p.Cat(rp.field, rp.fieldparam, f);
				RichText clip;
				clip.Cat(p);
				Insert(fieldpos, clip, false);
				Finish();
			}
			else {
				int64 l, h;
				if(GetWordSelection(c, l, h))
					SetSelection((int)l, (int)h);
			}
		}
	}
}

void WPEditor::LeftTriple(Point p, dword flags)
{
	NextUndo();
	int c = GetMousePos(p);
	if(c >= 0 && c != objectpos) {
		RichPos rp = text.GetRichPos(c);
		Select(c - rp.posinpara, rp.paralen + 1);
	}
}

Image WPEditor::CursorImage(Point p, dword flags)
{
	if(tablesel)
		return Image::Arrow();

	switch(GetHotSpot(p)) {
	case 0:
		return Image::SizeBottomRight();
	case 1:
		return Image::SizeVert();
	case 2:
		return Image::SizeHorz();
	default:
		if(text.GetRichPos(GetMousePos(p)).object) {
			return Image::Arrow();
		}
		else
		if(HasCapture() && tabmove.table && tabmove.column >= -2)
			return Image::SizeHorz();
		else {
			RichHotPos hp = GetHotPos(p);
			if(hp.table > 0)
				return Image::SizeHorz();
			else {
				int c = GetMousePos(p);
				return InSelection(c) && !HasCapture() ? Image::Arrow() : Image::IBeam();
			}
		}
	}
	return Image::Arrow();
}

void WPEditor::LeftRepeat(Point p, dword flags)
{
	NextUndo();
	if(HasCapture() && (flags & K_MOUSELEFT)) {
		if(p.y < 0)
			MoveUpDown(-1, true);
		if(p.y > GetSize().cy)
			MoveUpDown(1, true);
	}
}

//FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF


void WPEditor::FindReplaceAddHistory() {
	if(!String(~findreplace.find).IsEmpty())
		findreplace.find.AddHistory();
	if(!String(~findreplace.replace).IsEmpty())
		findreplace.replace.AddHistory();
}

void WPEditor::CloseFindReplace()
{
	if(!persistent_findreplace && findreplace.IsOpen())
		findreplace.Close();
}

bool compare3(const wchar *s, const wchar *a, const wchar *b, int len)
{
	const wchar *e = s + len;
	while(s < e) {
		if(*s != *a && *s != *b)
			return false;
		s++;
		a++;
		b++;
	}
	return true;
}

struct RichFindIterator : RichText::Iterator {
	int cursor;
	int fpos;
	WString upperw, lowerw;
	bool ww;
	int  len;

	virtual bool operator()(int pos, const RichPara& para)
	{
		WString ptext = para.GetText();
		if(pos + ptext.GetLength() > cursor && ptext.GetLength() >= len) {
			const wchar *q = ptext;
			const wchar *e = ptext.End() - len;
			if(cursor >= pos)
				q += cursor - pos;
			while(q <= e) {
				if(compare3(q, upperw, lowerw, len) &&
				   (!ww || (q + len == e || !IsLetter(q[len])) &&
				           (q == ptext || !IsLetter(q[-1])))) {
					fpos = int(q - ~ptext + pos);
					return true;
				}
				q++;
			}
		}
		return false;
	}
};

int  WPEditor::FindPos()
{
	RichFindIterator fi;
	WString w = findreplace.find.GetText();
	if(findreplace.ignorecase) {
		fi.upperw = ToUpper(w);
		fi.lowerw = ToLower(w);
	}
	else
		fi.upperw = fi.lowerw = w;
	fi.len = w.GetLength();
	fi.ww = findreplace.wholeword;
	if(w.GetLength()) {
		fi.cursor = cursor;
		if(text.Iterate(fi))
			return fi.fpos;
	}
	return -1;
}

void WPEditor::Find()
{
	CancelSelection();
	FindReplaceAddHistory();
	if(notfoundfw)
		Move(0, false);
	found = notfoundfw = false;
	int pos = FindPos();
	if(pos >= 0) {
		anchor = pos;
		cursor = pos + findreplace.find.GetText().GetLength();
		Finish();
		found = true;
		Size sz = findreplace.GetSize();
		Rect sw = GetScreenView();
		Rect r = sw.CenterRect(sz);
		Rect cr = GetCaretRect();
		if(cr.top < sz.cy + 2 * cr.Height()) {
			r.bottom = sw.bottom - 8;
			r.top = r.bottom - sz.cy;
		}
		else {
			r.top = sw.top + 24;
			r.bottom = r.top + sz.cy;
		}
		findreplace.SetRect(r);
		if(!findreplace.IsOpen()) {
			findreplace.Open();
		}
		SetFocus();
	}
	else {
		CancelSelection();
		CloseFindReplace();
		notfoundfw = true;
	}
}

RichText WPEditor::ReplaceText()
{
	RichText clip;
	RichPara p;
	formatinfo.ApplyTo(p.format);
	p.part.Add();
	formatinfo.ApplyTo(p[0].format);
	p.part.Top().text = findreplace.replace.GetText();
	clip.Cat(p);
	return clip;
}

void WPEditor::Replace()
{
	NextUndo();
	if(IsSelection() && found) {
		FindReplaceAddHistory();
		int c = min(cursor, anchor);
		Remove(c, abs(cursor - anchor));
		anchor = cursor = c;
		Insert(cursor, ReplaceText(), false);
		cursor += findreplace.replace.GetText().GetLength();
		anchor = cursor;
		Finish();
		Find();
	}
}

void WPEditor::OpenFindReplace()
{
	NextUndo();
	if(!findreplace.IsOpen()) {
		Size sz = findreplace.GetSize();
		findreplace.SetRect(GetScreenView().CenterRect(sz));
		int l, h;
		if(GetSelection(l, h)) {
			findreplace.amend.Hide();
			findreplace.ok.SetLabel(t_("Replace"));
			findreplace.Title(t_("Replace in selection"));
			findreplace.cancel <<= findreplace.Breaker(IDCANCEL);
			findreplace.ok <<= findreplace.Breaker(IDOK);
			if(findreplace.Execute() == IDOK) {
				int len = findreplace.find.GetText().GetLength();
				int rlen = findreplace.replace.GetText().GetLength();
				RichText rtext = ReplaceText();
				cursor = l;
				for(;;) {
					int pos = FindPos();
					if(pos < 0 || pos + len >= h)
						break;
					Select(pos, len);
					Remove(pos, len);
					Insert(pos, ReplaceText(), false);
					cursor += pos + rlen;
					h += rlen - len;
				}
				CancelSelection();
				Move(h, false);
			}
			FindReplaceAddHistory();
			findreplace.amend.Show();
			findreplace.ok.SetLabel(t_("Find"));
			findreplace.Title(t_("Find / Replace"));
			findreplace.cancel <<= callback(&findreplace, &TopWindow::Close);
			findreplace.ok <<= THISBACK(Find);
		}
		else {
			findreplace.Open();
			findreplace.find.SetFocus();
		}
	}
}

//GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG


void WPEditor::PasteFilter(RichText& txt, const String&) { Filter(txt); }
void WPEditor::Filter(RichText& txt) {}

void BegSelFixRaw(RichText& text)
{
	RichPos p = text.GetRichPos(0, 1);
	ASSERT(p.table == 1);
	if(p.table != 1)
		return;
	RichPara::Format fmt;
	text.InsertParaSpecial(1, true, fmt);
}

void BegSelUnFixRaw(RichText& text)
{
	ASSERT(text.GetLength() > 0);
	RichPos p = text.GetRichPos(1, 1);
	ASSERT(p.table == 1);
	if(p.table != 1)
		return;
	text.RemoveParaSpecial(1, true);
}

void WPEditor::UndoBegSelFix::Apply(RichText& txt)
{
	BegSelUnFixRaw(txt);
}

WPEditor::UndoRec *WPEditor::UndoBegSelFix::GetRedo(const RichText& txt)
{
	return new WPEditor::UndoBegSelUnFix;
}

void WPEditor::UndoBegSelUnFix::Apply(RichText& text)
{
	BegSelFixRaw(text);
}

WPEditor::UndoRec * WPEditor::UndoBegSelUnFix::GetRedo(const RichText& txt)
{
	return new WPEditor::UndoBegSelFix;
}

bool WPEditor::BegSelTabFix(int& count)
{
	if(begtabsel) { // If selection starts with first table which is the first element in the text
		int c = cursor;
		AddUndo(new UndoBegSelFix);
		BegSelFixRaw(text); // adds an empty paragraph at the start
		Move(0);
		Move(c + 1, true); // and changes the selection
		count++;
		begtabsel = false;
		return true;
	}
	return false;
}

void WPEditor::BegSelTabFixEnd(bool fix)
{ // removes empty paragraph added by BegSelTabFix
	if(fix && GetLength() > 0) {
		int c = cursor;
		AddUndo(new UndoBegSelUnFix);
		BegSelUnFixRaw(text);
		Move(0);
		Move(c - 1, true);
		begtabsel = true;
	}
}

bool WPEditor::InvalidRange(int l, int h)
{
	return !InSameTxt(text.GetRichPos(min(l, h)), text.GetRichPos(max(l, h)));
}

void WPEditor::AddUndo(UndoRec *ur)
{
	redo.Clear();
	SetModify();
	modified = true;
	incundoserial = true;
	while(undo.GetCount() > undosteps)
		undo.DropHead();
	found = false;
	ur->cursor = cursor;
	ur->serial = undoserial;
	undo.AddTail(ur);
}

void WPEditor::SaveStylesUndo()
{
	AddUndo(new UndoStyles(text));
}

void WPEditor::SaveStyleUndo(const Uuid& id)
{
	AddUndo(new UndoStyle(text, id));
}

void WPEditor::SaveFormat(int pos, int count)
{
	Limit(pos, count);
	AddUndo(new UndoFormat(text, pos, count));
}

void WPEditor::SaveFormat()
{
	int pos, count;
	if(IsSelection()) {
		if(tablesel) {
			SaveTable(tablesel);
			return;
		}
		pos = min(cursor, anchor);
		count = abs(cursor - anchor);
	}
	else {
		pos = cursor;
		count = 0;
	}
	bool b = BegSelTabFix(count);
	SaveFormat(pos, count);
	BegSelTabFixEnd(b);
}

void WPEditor::Limit(int& pos, int& count)
{
	int h = pos + count;
	pos = min(GetLength(), pos);
	count = min(GetLength(), h) - pos;
}

void WPEditor::ModifyFormat(int pos, const RichText::FormatInfo& fi, int count)
{
	if(IsReadOnly())
		return;
	bool b = BegSelTabFix(count);
	Limit(pos, count);
	SaveFormat(pos, count);
	text.ApplyFormatInfo(pos, fi, count);
	BegSelTabFixEnd(b);
}

void WPEditor::Remove(int pos, int len, bool forward)
{
	if(IsReadOnly())
		return;
	Limit(pos, len);
	if(InvalidRange(pos, pos + len))
		return;
	RichTxt::FormatInfo fi;
	if(forward)
		fi = text.GetFormatInfo(pos, 0);
	AddUndo(new UndoRemove(text, pos, len));
	text.Remove(pos, len);
	if(forward) {
		SaveFormat(pos, 0);
		text.ReplaceStyle(pos, fi.styleid);
		fi.paravalid &= ~RichText::STYLE;
		text.ApplyFormatInfo(pos, fi, 0);
	}
	SetModify();
	modified = true;
}

void WPEditor::Insert(int pos, const RichText& txt, bool typing)
{
	if(IsReadOnly())
		return;
	Index<int> lng;
	for(int i = 0; i < language.GetCount(); i++)
		lng.Add(language.GetKey(i));
	Vector<int> lngn = txt.GetAllLanguages();
	for(int i = 0; i < lngn.GetCount(); i++)
		lng.FindAdd(lngn[i]);
	SetupLanguage(lng.PickKeys());
	int l = text.GetLength();
	text.Insert(pos, txt);
	l = text.GetLength() - l;
	SetModify();
	modified = true;
	if(undo.GetCount()) {
		UndoRec& u = undo.Tail();
		if(typing) {
			UndoInsert *ui = dynamic_cast<UndoInsert *>(&u);
			if(ui && ui->length > 0 && ui->typing && ui->pos + ui->length == pos) {
				ui->length += l;
				return;
			}
		}
	}
	AddUndo(new UndoInsert(pos, l, typing));
}

void WPEditor::Undo()
{
	if(IsReadOnly())
		return;
	if(undo.IsEmpty()) return;
	CancelSelection();
	int serial = undo.Tail().serial;
	int c = cursor;
	while(undo.GetCount()) {
		UndoRec& u = undo.Tail();
		if(u.serial != serial) break;
		UndoRec *r = u.GetRedo(text);
		r->serial = u.serial;
		r->cursor = cursor;
		redo.Add(r);
		u.Apply(text);
		c = u.cursor;
		undo.DropTail();
		modified = true;
	}
	ReadStyles();
	Move(c);	
}

void WPEditor::Redo()
{
	if(IsReadOnly())
		return;
	if(redo.IsEmpty()) return;
	NextUndo();
	CancelSelection();
	int serial = redo.Top().serial;
	int c = cursor;
	while(redo.GetCount()) {
		UndoRec& r = redo.Top();
		if(r.serial != serial) break;
		UndoRec *u = r.GetRedo(text);
		u->serial = r.serial;
		u->cursor = cursor;
		undo.AddTail(u);
		r.Apply(text);
		c = r.cursor;
		redo.Drop();
		modified = true;
	}
	ReadStyles();
	Move(c);
}

#ifdef PLATFORM_WIN32
#define RTFS "Rich Text Format"
#else
#define RTFS "text/richtext"
#endif

RichText WPEditor::GetSelection(int maxcount) const
{
	RichText clip;
	if(tablesel) {
		RichTable tab = text.CopyTable(tablesel, cells);
		clip.SetStyles(text.GetStyles());
		clip.CatPick(pick(tab));
	}
	else {
		if(begtabsel) {
			int pos = 0;
			RichPos p = text.GetRichPos(0, 1);
			if(p.table) {
				clip.SetStyles(text.GetStyles());
				do {
					RichTable tab = text.CopyTable(p.table);
					clip.CatPick(pick(tab));
					pos += p.tablen + 1;
					p = text.GetRichPos(pos, 1);
				}
				while(p.table);
				clip.CatPick(text.Copy(pos, minmax(abs(cursor - pos), 0, maxcount)));
			}
		}
		else
			clip = text.Copy(min(cursor, anchor), min(maxcount, abs(cursor - anchor)));
	}
	return clip;
}

void WPEditor::Cut()
{
	if(IsReadOnly())
		return;
	Copy();
	if(IsSelection())
		RemoveSelection();
	else
	if(objectpos >= 0) {
		Remove(cursor, 1);
		Move(cursor, false);
	}
}

void WPEditor::PasteText(const RichText& text)
{
	SetModify();
	modified = true;
	RemoveSelection();
	Insert(cursor, text, false);
	ReadStyles();
	Move(cursor + text.GetLength(), false);
}

struct ToParaIterator : RichText::Iterator {
	RichPara para;
	bool     space;

	virtual bool operator()(int pos, const RichPara& p) {
		for(int i = 0; i < p.GetCount(); i++) {
			const RichPara::Part& part = p[i];
			if(part.IsText()) {
				const wchar *s = part.text;
				while(*s) {
					while(*s && *s <= ' ') {
						space = true;
						s++;
					}
					const wchar *t = s;
					while(*s > ' ') s++;
					if(s > t) {
						if(space)
							para.Cat(" ", part.format);
						para.Cat(WString(t, s), part.format);
						space = false;
					}
				}
			}
			else if(!part.field.IsNull()) {
				para.Cat(part.field, part.fieldparam, part.format);
				space = false;
			}
			else if(part.object) {
				para.Cat(part.object, part.format);
				space = false;
			}
		}
		space = true;
		return false;
	}

	ToParaIterator() { space = false; }
};

void WPEditor::ToPara()
{
	if(IsReadOnly())
		return;
	if(!IsSelection() || tablesel)
		return;
	NextUndo();
	RichText txt = text.Copy(min(cursor, anchor), abs(cursor - anchor));
	ToParaIterator it;
	txt.Iterate(it);
	RichText h;
	h.SetStyles(txt.GetStyles());
	h.Cat(it.para);
	PasteText(h);
}

void WPEditor::RemoveText(int count)
{
	CancelSelection();
	Remove(cursor, count);
	Finish();
}

RichText WPEditor::CopyText(int pos, int count) const
{
	return text.Copy(pos, count);
}

void WPEditor::InsertObject(int type)
{
	RichObjectType& richtype = RichObject::GetType(type);
	RichObject object = RichObject(&richtype, Value());
	RichObject o = object;
	o.DefaultAction(context);
	if(o.GetSerialId() != object.GetSerialId()) {
		RichText::FormatInfo finfo = GetFormatInfo();
		RemoveSelection();
		RichPara p;
		p.Cat(o, finfo);
		RichText clip;
		clip.Cat(p);
		Insert(GetCursor(), clip, false);
		Finish();
	}
}

void WPEditor::ReplaceObject(const RichObject& obj)
{
	Remove(objectpos, 1);
	RichPara p;
	p.Cat(obj, formatinfo);
	RichText clip;
	clip.Cat(p);
	Insert(objectpos, clip, false);
	Finish();
	objectrect = GetObjectRect(objectpos);
}

RichObject WPEditor::GetObject() const
{
	return text.GetRichPos(objectpos).object;
}

void WPEditor::Select(int pos, int count)
{
	found = false;
	Move(pos);
	Move(pos + count, true);
}

void WPEditor::InsertLine()
{
	if(IsReadOnly())
		return;
	RichText::FormatInfo b = formatinfo;
	RichText h;
	h.SetStyles(text.GetStyles());
	RichPara p;
	p.format = formatinfo;
	h.Cat(p);
	h.Cat(p);
	bool st = cursorp.paralen == cursorp.posinpara;
	bool f = cursorp.posinpara == 0 && formatinfo.label.GetCount();
	Insert(cursor, h, false);
	if(f) {
		String lbl = formatinfo.label;
		formatinfo.label.Clear();
		ApplyFormat(0, RichText::LABEL);
		formatinfo.label = lbl;
	}
	anchor = cursor = cursor + 1;
	begtabsel = false;
	formatinfo.newpage = formatinfo.newhdrftr = false;
	if(st) {
		Uuid next = text.GetStyle(b.styleid).next;
		if(next != formatinfo.styleid) {
			formatinfo.label.Clear();
			formatinfo.styleid = next;
			ApplyFormat(0, RichText::STYLE|RichText::NEWPAGE|RichText::LABEL|RichText::NEWHDRFTR);
			return;
		}
	}
	ApplyFormat(0, RichText::NEWPAGE|RichText::LABEL|RichText::NEWHDRFTR);
	objectpos = -1;
}

//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH


//#define RTFS "Rich Text Format;text/rtf;application/rtf"

void WPEditor::InsertImage()
{
	if(!imagefs.ExecuteOpen(t_("Open image from file")))
		return;
	String fn = ~imagefs;
	if(GetFileLength(fn) > 17000000) {
		Exclamation("Image is too large!");
		return;
	}
	String data = LoadFile(fn);
	StringStream ss(data);
	if(!StreamRaster::OpenAny(ss)) {
		Exclamation(NFormat(t_("Unsupported image format in file [* \1%s\1]."), ~imagefs));
		return;
	}
	RichText clip;
	RichPara p;
	p.Cat(CreateRawImageObject(data), formatinfo);
	clip.Cat(p);
	ClipPaste(clip, "image/raw");
}

bool WPEditor::Accept(PasteClip& d, RichText& clip, String& fmt)
{
	if(IsReadOnly())
		return false;
	if(AcceptFiles(d)) {
		Vector<String> s = GetFiles(d);
		if(s.GetCount()) {
			String fn = s[0];
			String ext = ToUpper(GetFileExt(fn));
			if(ext == ".PNG" || ext == ".JPG" || ext == ".JPEG" || ext == ".GIF" || ext == ".TIF" || ext == ".TIFF") {
				if(d.Accept()) {
					if(StreamRaster::LoadFileAny(fn)) {
						RichPara p;
						p.Cat(CreateRawImageObject(LoadFile(fn)), formatinfo);
						clip.Cat(p);
						fmt = "files";
					}
					return true;
				}
				return false;
			}
		}
		d.Reject();
	}
	if(d.Accept("text/QTF")) {
		fmt = "text/QTF";
		clip = ParseQTF(~d, 0, context);
		return true;
	}
	if(d.Accept(RTFS)) {
		fmt = RTFS;
		clip = ParseRTF(~d);
		return true;
	}
	for(int i = 0; i < RichObject::GetTypeCount(); i++) {
		RichObjectType& rt = RichObject::GetType(i);
		if(rt.Accept(d)) {
			Value data = rt.Read(d);
			if(!IsNull(data)) {
				RichPara p;
				RichObject o = RichObject(&rt, data, pagesz);
				p.Cat(o, formatinfo);
				clip.Cat(p);
				fmt = o.GetTypeName();
			}
			return true;
		}
	}
	if(AcceptText(d)) {
		fmt = "text/plain";
		clip = AsRichText(GetWString(d), formatinfo);
		return true;
	}
	return false;
}

void WPEditor::ClipPaste(RichText& clip, const String& fmt)
{
	clip.ApplyZoom(clipzoom.Reciprocal());
	PasteFilter(clip, fmt);
	NextUndo();
	if(clip.GetPartCount() == 1 && clip.IsTable(0)) {
		CancelSelection();
		if(cursorp.table) {
			NextUndo();
			SaveTable(cursorp.table);
			text.PasteTable(cursorp.table, cursorp.cell, clip.GetTable(0));
			Finish();
			return;
		}
	}
	clip.Normalize();
	PasteText(clip);
}

void WPEditor::DragAndDrop(Point p, PasteClip& d)
{
	int dropcursor = GetMousePos(p);
	if(dropcursor >= 0) {
		RichText clip;
		String fmt;
		if(Accept(d, clip, fmt)) {
			NextUndo();
			int a = sb;
			int c = dropcursor;
			if(InSelection(c)) {
				if(!IsReadOnly())
					RemoveSelection();
				if(IsDragAndDropSource())
					d.SetAction(DND_COPY);
			}
			int sell, selh;
			if(GetSelection(sell, selh) && d.GetAction() == DND_MOVE && IsDragAndDropSource()) {
				if(c > sell)
					c -= selh - sell;
				if(!IsReadOnly())
					RemoveSelection();
				d.SetAction(DND_COPY);
			}
			Move(c);
			clip.Normalize();
			ClipPaste(clip, fmt);
			sb = a;
			Select(c, clip.GetLength());
			SetFocus();
			Action();
			return;
		}
	}
	if(!d.IsAccepted())
		dropcursor = -1;
	Rect r = Null;
	if(dropcursor >= 0 && dropcursor < text.GetLength()) {
		RichCaret pr = text.GetCaret(dropcursor, pagesz);
		Zoom zoom = GetZoom();
		Rect tr = GetTextRect();
		r = RectC(pr.left * zoom + tr.left - 1,
		          GetPosY(pr) + (pr.lineascent - pr.caretascent) * zoom,
		          2, (pr.caretascent + pr.caretdescent) * zoom);
	}
	if(r != dropcaret) {
		RefreshDropCaret();
		dropcaret = r;
		RefreshDropCaret();
	}
}

void WPEditor::DragRepeat(Point p)
{
	sb = (int)sb + GetDragScroll(this, p, 16).y;
}

void WPEditor::RefreshDropCaret()
{
	Refresh(dropcaret.OffsetedVert(-sb));
}

void WPEditor::Paste()
{
	if(IsReadOnly())
		return;
	RichText clip;
	PasteClip d = Clipboard();
	String fmt;
	if(!Accept(d, clip, fmt))
		return;
	ClipPaste(clip, fmt);
}

void WPEditor::DragLeave()
{
	RefreshDropCaret();
	dropcaret.Clear();
}

static String sRTF(const Value& data)
{
	const RichText& txt = ValueTo<RichText>(data);
	return EncodeRTF(txt);
}

static String sQTF(const Value& data)
{
	const RichText& txt = ValueTo<RichText>(data);
	return AsQTF(txt);
}

void WPEditor::ZoomClip(RichText& text) const
{
	text.ApplyZoom(clipzoom);
}

void AppendClipboard(RichText&& txt)
{
	AppendClipboardUnicodeText(txt.GetPlainText());
	Value clip = RawPickToValue(pick(txt));
	AppendClipboard("text/QTF", clip, sQTF);
	AppendClipboard(RTFS, clip, sRTF);
}

void WPEditor::Copy()
{
	RichText txt;
	if(IsSelection())
		txt = GetSelection();
	else if(objectpos >= 0)
		txt = text.Copy(cursor, 1);
	else {
		BeepExclamation();
		return;
	}
	ZoomClip(txt);
	ClearClipboard();
	AppendClipboard(pick(txt));
	if(objectpos >= 0) {
		RichObject o = GetObject();
		Vector<String> v = Split(o.GetType().GetClipFmts(), ';');
		for(int i = 0; i < v.GetCount(); i++)
			AppendClipboard(v[i], o.GetType().GetClip(o.GetData(), v[i]));
	}
}

String WPEditor::GetSelectionData(const String& fmt) const
{
	String f = fmt;
	if(IsSelection()) {
		RichText clip = GetSelection();
		ZoomClip(clip);
		if(f == "text/QTF")
			return AsQTF(clip);
		if(InScList(f, RTFS))
			return EncodeRTF(clip);
		return GetTextClip(clip.GetPlainText(), fmt);
	}
/*	else
	if(objectpos >= 0) {
		RichObject o = GetObject();
		if(InScList(fmt, o.GetType().GetClipFmts()))
			return o.GetType().GetClip(o.GetData(), fmt);
	}*/
	return Null;
}

void WPEditor::LeftDrag(Point p, dword flags)
{
	int c = GetMousePos(p);
	Size ssz = StdSampleSize();
	if(!HasCapture() && InSelection(c)) {
		RichText sample = GetSelection(5000);
		sample.ApplyZoom(Zoom(1, 8));
		ImageDraw iw(ssz);
		iw.DrawRect(0, 0, ssz.cx, ssz.cy, White);
		sample.Paint(iw, 0, 0, 128);
		NextUndo();
		if(DoDragAndDrop(String().Cat() << "text/QTF;" RTFS ";" << ClipFmtsText(),
		                 ColorMask(iw, White)) == DND_MOVE && !IsReadOnly()) {
			RemoveSelection();
			Action();
		}
	}
/*	else
	if(objectpos >= 0 && c == objectpos) {
		ReleaseCapture();
		RichObject o = GetObject();
		Size sz = o.GetPhysicalSize();
		NextUndo();
		if(DoDragAndDrop(o.GetType().GetClipFmts(),
		                 o.ToImage(Size(ssz.cx, sz.cy * ssz.cx / sz.cx))) == DND_MOVE
		   && objectpos >= 0) {
			if(droppos > objectpos)
				droppos--;
			Remove(objectpos, 1);
		}
		Move(droppos);
		SetObjectPos(droppos);
	}*/
}

void  WPEditor::MiddleDown(Point p, dword flags)
{
	RichText clip;
	if(IsReadOnly())
		return;
	String fmt;
	if(Accept(Selection(), clip, fmt)) {
		selclick = false;
		LeftDown(p, flags);
		ClipPaste(clip, fmt);
	}
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII


void WPEditor::SaveTableFormat(int table)
{
	AddUndo(new UndoTableFormat(text, table));
}

void WPEditor::SaveTable(int table)
{
	AddUndo(new UndoTable(text, table));
}

void WPEditor::InsertTable()
{
	if(IsSelection())
		return;
	WithCreateTableLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, t_("Insert table"));
	dlg.header = false;
	dlg.columns <<= 2;
	dlg.columns.MinMax(1, 20);
	dlg.ActiveFocus(dlg.columns);
	if(dlg.Run() != IDOK)
		return;
	RichTable::Format fmt;
	int nx = minmax((int)~dlg.columns, 1, 20);
	for(int q = nx; q--;)
		fmt.column.Add(1);
	if(dlg.header)
		fmt.header = 1;
	RichTable table;
	table.SetFormat(fmt);
	for(int i = 0; i < (dlg.header ? 2 : 1); i++)
		for(int j = 0; j < nx; j++) {
			RichText h;
			h.SetStyles(text.GetStyles());
			RichPara p;
			p.format = formatinfo;
			p.format.newpage = false;
			p.format.label.Clear();
			h.Cat(p);
			table.SetPick(i, j, pick(h));
		}
	NextUndo();
	if(cursorp.posinpara)
		InsertLine();
	if(text.GetRichPos(cursor).paralen) {
		InsertLine();
		cursor = anchor = cursor - 1;
		begtabsel = false;
	}
	SaveFormat(cursor, 0);
	AddUndo(new UndoCreateTable(text.SetTable(cursor, table)));
	Finish();
}

template <class T>
struct CtrlRetrieveItemValueNN : public CtrlRetriever::Item {
	Ctrl& ctrl;
	T&    value;

	virtual void Retrieve() {
		if(!IsNull(ctrl))
			value = ~ctrl;
	}

	CtrlRetrieveItemValueNN(Ctrl& ctrl, T& value) : ctrl(ctrl), value(value) {}
};

template <class T>
void Advn(CtrlRetriever& r, Ctrl& ctrl, T& value) {
	ctrl <<= value;
	r.Put(new CtrlRetrieveItemValueNN<T>(ctrl, value));
}

void WPEditor::DestroyTable()
{
	AddUndo(new UndoDestroyTable(text, cursorp.table));
	int c = text.GetCellPos(cursorp.table, 0, 0).pos;
	text.DestroyTable(cursorp.table);
	Move(c);
}

int CharFilterEqualize(int c)
{
	return IsDigit(c) || c == ':' ? c : 0;
}

struct RichEditTableProperties : WithTablePropertiesLayout<TopWindow> {
	String header_qtf, footer_qtf;

	void EditHdrFtr()
	{
		WPEditHeaderFooter(header_qtf, footer_qtf);
	}
	
	void NewHdrFtr()
	{
		SyncHdrFtr();
		if(newhdrftr)
			EditHdrFtr();
	}
	
	void SyncHdrFtr()
	{
		hdrftr.Enable(newhdrftr && newhdrftr.IsEnabled());
	}
	
	typedef RichEditTableProperties CLASSNAME;

	RichEditTableProperties() {
		CtrlLayoutOKCancel(*this, t_("Table properties"));
		newhdrftr <<= THISBACK(NewHdrFtr);
		hdrftr <<= THISBACK(EditHdrFtr);
		SyncHdrFtr();
	}
};

void WPEditor::TableProps()
{
	if(IsSelection() || cursorp.table == 0)
		return;
	RichEditTableProperties dlg;
	dlg.Breaker(dlg.destroy, IDNO);
	RichTable::Format fmt = text.GetTableFormat(cursorp.table);
	String ratios;
	for(int i = 0; i < fmt.column.GetCount(); i++) {
		if(i)
			ratios << ':';
		ratios << "1";
	}
	dlg.ratios.SetFilter(CharFilterEqualize);
	dlg.ratios <<= ratios;
	CtrlRetriever r;
	Advn(r, dlg.before.SetUnit(unit), fmt.before);
	Advn(r, dlg.after.SetUnit(unit), fmt.after);
	Advn(r, dlg.lm.SetUnit(unit), fmt.lm);
	Advn(r, dlg.rm.SetUnit(unit), fmt.rm);
	Advn(r, dlg.frame.SetUnit(unit), fmt.frame);
	r(dlg.framecolor, fmt.framecolor);
	Advn(r, dlg.grid.SetUnit(unit), fmt.grid);
	Advn(r, dlg.header, fmt.header);
	Advn(r, dlg.keep, fmt.keep);
	Advn(r, dlg.newpage, fmt.newpage);
	Advn(r, dlg.newhdrftr, fmt.newhdrftr);
	dlg.header_qtf = fmt.header_qtf;
	dlg.footer_qtf = fmt.footer_qtf;
	r(dlg.gridcolor, fmt.gridcolor);
	dlg.SyncHdrFtr();
	dlg.newhdrftr.Enable(cursorp.level == 1);
	dlg.hdrftr.Enable(cursorp.level == 1);
	for(;;) {
		switch(dlg.Run()) {
		case IDCANCEL:
			return;
		case IDNO:
			NextUndo();
			DestroyTable();
			return;
		default:
			r.Retrieve();
			if(dlg.newhdrftr) {
				fmt.header_qtf = dlg.header_qtf;
				fmt.footer_qtf = dlg.footer_qtf;
			}
			else
				fmt.header_qtf = fmt.footer_qtf = Null;
			const RichTable& tbl = text.GetConstTable(cursorp.table);
			bool valid = true;
			Point violator(0, 0);
			int vspan = 0;
			for(int rw = 0; valid && rw < fmt.header && rw < tbl.GetRows(); rw++)
				for(int co = 0; valid && co < tbl.GetColumns(); co++)
					if(tbl(rw, co) && (vspan = tbl[rw][co].vspan) + rw > fmt.header) {
						valid = false;
						violator.x = co;
						violator.y = rw;
						break;
					}
			if(!valid) {
				Exclamation(NFormat(t_("Invalid header row count %d, cell at rw %d, co %d has vspan = %d."),
					fmt.header, violator.y + 1, violator.x + 1, vspan));
				continue;
			}
			NextUndo();
			SaveTableFormat(cursorp.table);
			if(dlg.equalize) {
				Vector<String> r = Split((String)~dlg.ratios, ':');
				for(int i = 0; i < fmt.column.GetCount(); i++)
					fmt.column[i] = i < r.GetCount() ? max(atoi(r[i]), 1) : 1;
			}
			text.SetTableFormat(cursorp.table, fmt);
			Finish();
			return;
		}
	}
}

bool WPEditor::RemoveSpecial(int ll, int hh, bool back)
{
	NextUndo();
	int l = min(ll, hh);
	int h = max(ll, hh);
	RichPos p1 = text.GetRichPos(l);
	RichPos p2 = text.GetRichPos(h);
	if(InSameTxt(p1, p2))
		return false;
	if(p1.paralen == 0 && p2.posintab == 0 && text.CanRemoveParaSpecial(p2.table, true)) {
		AddUndo(new UndoRemoveParaSpecial(text, p2.table, true));
		text.RemoveParaSpecial(p2.table, true);
		Move(cursor - back);
	}
	else
	if(p2.paralen == 0 && p1.posintab == p1.tablen && text.CanRemoveParaSpecial(p1.table, false)) {
		AddUndo(new UndoRemoveParaSpecial(text, p1.table, false));
		text.RemoveParaSpecial(p1.table, false);
		Move(cursor - back);
	}
	return true;
}

bool WPEditor::InsertLineSpecial()
{
	NextUndo();
	if(cursorp.table) {
		RichPara::Format fmt;
		fmt = formatinfo;
		fmt.newpage = false;
		fmt.label.Clear();
		if(cursorp.posintab == 0 && text.ShouldInsertParaSpecial(cursorp.table, true)) {
			AddUndo(new UndoInsertParaSpecial(cursorp.table, true));
			text.InsertParaSpecial(cursorp.table, true, fmt);
			Move(cursor + 1);
			return true;
		}
		if(cursorp.posintab == cursorp.tablen && text.ShouldInsertParaSpecial(cursorp.table, false)) {
			AddUndo(new UndoInsertParaSpecial(cursorp.table, false));
			text.InsertParaSpecial(cursorp.table, false, fmt);
			Move(cursor + 1);
			return true;
		}
	}
	return false;
}

void WPEditor::TableInsertRow()
{
	if(IsSelection() || !cursorp.table)
		return;
	NextUndo();
	SaveTable(cursorp.table);
	Point p = cursorp.cell;
	if(cursorp.posintab == cursorp.tablen) {
		p.y++;
		p.x = 0;
	}
	text.InsertTableRow(cursorp.table, p.y);
	Move(text.GetCellPos(cursorp.table, text.GetMasterCell(cursorp.table, p)).pos);
}

void WPEditor::TableRemoveRow()
{

	if(IsSelection() || !cursorp.table)
		return;
	NextUndo();
	if(cursorp.tabsize.cy <= 1)
		DestroyTable();
	else {
		SaveTable(cursorp.table);
		text.RemoveTableRow(cursorp.table, cursorp.cell.y);
		Move(text.GetCellPos(cursorp.table, text.GetMasterCell(cursorp.table, cursorp.cell)).pos);
	}
}

void WPEditor::TableInsertColumn()
{
	if(IsSelection() || !cursorp.table)
		return;
	NextUndo();
	SaveTable(cursorp.table);
	Point p = cursorp.cell;
	if(cursorp.cell.x == cursorp.tabsize.cx - 1 && cursorp.posincell == cursorp.celllen)
		p.x++;
	text.InsertTableColumn(cursorp.table, p.x);
	Move(text.GetCellPos(cursorp.table, text.GetMasterCell(cursorp.table, p)).pos);
}

void WPEditor::TableRemoveColumn()
{

	if(IsSelection() || !cursorp.table)
		return;
	NextUndo();
	if(cursorp.tabsize.cx <= 1)
		DestroyTable();
	else {
		SaveTable(cursorp.table);
		text.RemoveTableColumn(cursorp.table, cursorp.cell.x);
		Move(text.GetCellPos(cursorp.table, text.GetMasterCell(cursorp.table, cursorp.cell)).pos);
	}
}

void WPEditor::SplitCell()
{
	if(IsSelection() || !cursorp.table)
		return;
	WithSplitCellLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, t_("Split cell"));
	dlg.cx.MinMax(1, 20).NotNull();
	dlg.cx <<= 1;
	dlg.cy.MinMax(1, 20).NotNull();
	dlg.cy <<= 1;
	if(dlg.Execute() != IDOK)
		return;
	NextUndo();
	SaveTable(cursorp.table);
	text.SplitCell(cursorp.table, cursorp.cell, Size(~dlg.cx, ~dlg.cy));
	Finish();
}

void WPEditor::CellProperties()
{
	if(!(tablesel || cursorp.table && !IsSelection()))
		return;
	WithCellPropertiesLayout<TopWindow> dlg;
	CtrlLayoutOKCancel(dlg, t_("Cell properties"));
	int  tab;
	Rect a;
	if(tablesel) {
		tab = tablesel;
		a = cells;
	}
	else {
		tab = cursorp.table;
		a = Rect(cursorp.cell, Size(0, 0));
	}
	RichCell::Format fmt = text.GetCellFormat(tab, a);
	CtrlRetriever r;
	r
		(dlg.leftb.SetUnit(unit), fmt.border.left)
		(dlg.rightb.SetUnit(unit), fmt.border.right)
		(dlg.topb.SetUnit(unit), fmt.border.top)
		(dlg.bottomb.SetUnit(unit), fmt.border.bottom)
		(dlg.leftm.SetUnit(unit), fmt.margin.left)
		(dlg.rightm.SetUnit(unit), fmt.margin.right)
		(dlg.topm.SetUnit(unit), fmt.margin.top)
		(dlg.bottomm.SetUnit(unit), fmt.margin.bottom)
		(dlg.align, fmt.align)
		(dlg.minheight.SetUnit(unit), fmt.minheight)
		(dlg.color, fmt.color)
		(dlg.border, fmt.bordercolor)
		(dlg.keep, fmt.keep)
		(dlg.round, fmt.round)
	;
	dlg.align.Set(0, ALIGN_TOP);
	dlg.align.Set(1, ALIGN_CENTER);
	dlg.align.Set(2, ALIGN_BOTTOM);
	dlg.color.WithVoid().VoidText(t_("(no change)"));
	dlg.border.WithVoid().VoidText(t_("(no change)"));
	if(tablesel) {
		dlg.keep.ThreeState();
		dlg.keep <<= Null;
		dlg.round.ThreeState();
		dlg.round <<= Null;
	}
	if(dlg.Run() != IDOK)
		return;
	r.Retrieve();
	NextUndo();
	SaveTable(tab);
	text.SetCellFormat(tab, a, fmt, !tablesel || !IsNull(dlg.keep), !tablesel || !IsNull(dlg.round));
	Finish();
}

void WPEditor::JoinCell()
{
	if(!tablesel)
		return;
	NextUndo();
	SaveTable(tablesel);
	text.JoinCell(tablesel, cells);
	Move(text.GetCellPos(tablesel, cells.TopLeft()).pos);
}

//JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ


struct WPHeaderFooter : public WPEditor {
	ToolBar  toolbar;

	void RefreshBar();
	void TheBar(Bar& bar);
	
	void  PageNumber();
	void  PageCount();
	
	void  Init(int pageno, int pagecount);

	typedef WPHeaderFooter CLASSNAME;

	WPHeaderFooter();
};

void WPHeaderFooter::TheBar(Bar& bar)
{
	EditTools(bar);
	bar.Gap();
	FontTools(bar);
	bar.Gap();
	InkTool(bar);
	PaperTool(bar);
	bar.Gap();
	LanguageTool(bar);
	SpellCheckTool(bar);
	bar.Break();
	StyleTool(bar);
	bar.Gap();
	ParaTools(bar);
	bar.Gap();
	TableTools(bar);
	bar.Gap();
	bar.Add(!IsReadOnly(), t_("Insert page number"), WPEditCtrlImg::PageNumber(), THISBACK(PageNumber));
	bar.Add(!IsReadOnly(), t_("Insert page count"), WPEditCtrlImg::PageCount(), THISBACK(PageCount));
}

void WPHeaderFooter::PageNumber()
{
	PasteText(ParseQTF("{:VALUE:PAGENUMBER:}"));
	EvaluateFields();
}

void WPHeaderFooter::PageCount()
{
	PasteText(ParseQTF("{:VALUE:PAGECOUNT:}"));
	EvaluateFields();
}

void WPHeaderFooter::RefreshBar()
{
	toolbar.Set(THISBACK(TheBar));
}

WPHeaderFooter::WPHeaderFooter()
{
	InsertFrame(0, toolbar);
	WhenRefreshBar = callback(this, &WPHeaderFooter::RefreshBar);
	SetVar("PAGECOUNT", "###");
	SetVar("PAGENUMBER", "#");
	SetVar("__DISPLAY_VALUE_FIELDS", 1);
}

struct HeaderFooterDlg : WithHeaderFooterLayout<TopWindow> {
	typedef HeaderFooterDlg CLASSNAME;
	
	WPHeaderFooter header_editor, footer_editor;
	
	void Sync();
	void Set(const String& header_qtf, const String& footer_qtf);
	void Get(String& header_qtf, String& footer_qtf);
	void Load(const RichText& text);
	void Save(RichText& text);

	HeaderFooterDlg();
};

void HeaderFooterDlg::Sync()
{
	header_editor.Enable(use_header);
	footer_editor.Enable(use_footer);
}

HeaderFooterDlg::HeaderFooterDlg()
{
	CtrlLayoutOKCancel(*this, "Header / Footer");
	use_header <<= use_footer <<= THISBACK(Sync);
	Sync();
}

void HeaderFooterDlg::Set(const String& header_qtf, const String& footer_qtf)
{
	use_header = !IsNull(header_qtf);
	if(use_header)
		header_editor.SetQTF(header_qtf);
	header_editor.EvaluateFields();
	use_footer = !IsNull(footer_qtf);
	if(use_footer)
		footer_editor.SetQTF(footer_qtf);
	footer_editor.EvaluateFields();
	Sync();
}

void HeaderFooterDlg::Get(String& header_qtf, String& footer_qtf)
{
	header_qtf = use_header ? header_editor.GetQTF() : String();
	footer_qtf = use_footer ? footer_editor.GetQTF() : String();
}

void HeaderFooterDlg::Load(const RichText& text)
{
	Set(text.GetHeaderQtf(), text.GetFooterQtf());
}

void HeaderFooterDlg::Save(RichText& text)
{
	if(use_header)
		text.SetHeaderQtf(header_editor.GetQTF());
	else
		text.ClearHeader();
	if(use_footer)
		text.SetFooterQtf(footer_editor.GetQTF());
	else
		text.ClearFooter();
}

void WPEditor::HeaderFooter()
{
	HeaderFooterDlg dlg;
	dlg.Load(text);
	dlg.SetRect(0, 0, GetSize().cx, dlg.GetLayoutSize().cy);
	if(dlg.Execute() == IDOK) {
		dlg.Save(text);
		Refresh();
		Finish();
	}
}

bool WPEditHeaderFooter(String& header_qtf, String& footer_qtf)
{
	HeaderFooterDlg dlg;
	dlg.Set(header_qtf, footer_qtf);
	if(dlg.Execute() == IDOK) {
		dlg.Get(header_qtf, footer_qtf);
		return true;
	}
	return false;
}

//KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK


RichPara::NumberFormat WPParaFormatting::GetNumbering()
{
	RichPara::NumberFormat f;
	f.before_number = ~before_number;
	f.after_number = ~after_number;
	f.reset_number = ~reset_number;
	for(int i = 0; i < 8; i++)
		f.number[i] = Nvl((int)~n[i]);
	return f;
}

bool WPParaFormatting::IsNumbering()
{
	if(!IsNull(before_number) || !IsNull(after_number) || reset_number)
		return true;
	for(int i = 0; i < 8; i++)
		if(!IsNull(n[i]))
		   return true;
	return false;
}

void WPParaFormatting::EnableNumbering()
{
	bool b = !IsNull(bullet) && !(int)~bullet;
	before_number.Enable(b);
	after_number.Enable(b);
	for(int i = 0; i < 8; i++)
		n[i].Enable(b);
}

int  WPParaFormatting::ComputeIndent()
{
	if(!IsNull(bullet) && (int)~bullet)
		return 150;
	if(IsNumbering()) {
		RichPara::NumberFormat f = GetNumbering();
		RichPara::Number n;
		static byte n0[] = { 0, 37, 38, 48, 48, 37, 37 };
		static byte n1[] = { 0, 17, 18, 12, 12, 17, 17 };
		if(f.number[0] && f.number[0] < 8)
			n.n[0] = n0[f.number[0]];
		if(f.number[1] && f.number[1] < 8)
			n.n[1] = n1[f.number[1]];
		for(int i = 2; i < 8; i++) {
			static byte nn[] = { 0,  7,  8,  1,  1,  7,  7 };
			if(f.number[i] && f.number[i] < 8)
				n.n[i] = nn[f.number[i]];
		}
		String s = n.AsText(f);
		s.Cat(' ');
		return GetTextSize(s, font).cx;
	}
	return 0;
}

void WPParaFormatting::SetupIndent()
{
	EnableNumbering();
	if(indent.IsModified() || keepindent) return;
	indent <<= ComputeIndent();
	indent.ClearModify();
}

void WPParaFormatting::EditHdrFtr()
{
	if (WPEditHeaderFooter(header_qtf, footer_qtf))
		modified = true;
}

void WPParaFormatting::NewHdrFtr()
{
	SyncHdrFtr();
	if(newhdrftr)
		EditHdrFtr();
}

void WPParaFormatting::SyncHdrFtr()
{
	hdrftr.Enable(newhdrftr && newhdrftr.IsEnabled());
}

void WPParaFormatting::Set(int unit, const RichText::FormatInfo& formatinfo, bool baselevel)
{
	newhdrftr.Enable(baselevel);
	hdrftr.Enable(baselevel);
	font = formatinfo;
	ruler.Set(unit, RichText::RULER & formatinfo.paravalid ? formatinfo.ruler : Null);
	before.Set(unit, RichText::BEFORE & formatinfo.paravalid ? formatinfo.before : Null);
	lm.Set(unit, RichText::LM & formatinfo.paravalid ? formatinfo.lm : Null);
	indent.Set(unit, RichText::INDENT & formatinfo.paravalid ? formatinfo.indent : Null);
	rm.Set(unit, RichText::RM & formatinfo.paravalid ? formatinfo.rm : Null);
	after.Set(unit, RichText::AFTER & formatinfo.paravalid ? formatinfo.after : Null);
	if(RichText::ALIGN & formatinfo.paravalid)
		align <<= formatinfo.align == ALIGN_LEFT    ? 0 :
		          formatinfo.align == ALIGN_CENTER  ? 1 :
		          formatinfo.align == ALIGN_RIGHT   ? 2 :
		                                            3;
	if(RichText::NEWHDRFTR & formatinfo.paravalid) {
		newhdrftr = formatinfo.newhdrftr;
		header_qtf = formatinfo.header_qtf;
		footer_qtf = formatinfo.footer_qtf;
	}
	else {
		newhdrftr = Null;
		newhdrftr.ThreeState();
	}
	if(RichText::NEWPAGE & formatinfo.paravalid)
		page = formatinfo.newpage;
	else {
		page = Null;
		page.ThreeState();
	}
	if(RichText::KEEP & formatinfo.paravalid)
		keep = formatinfo.keep;
	else {
		keep = Null;
		keep.ThreeState();
	}
	if(RichText::KEEPNEXT & formatinfo.paravalid)
		keepnext = formatinfo.keepnext;
	else {
		keepnext = Null;
		keepnext.ThreeState();
	}
	if(RichText::ORPHAN & formatinfo.paravalid)
		orphan = formatinfo.orphan;
	else {
		orphan = Null;
		orphan.ThreeState();
	}
	if(RichText::RULERINK & formatinfo.paravalid)
		rulerink <<= formatinfo.rulerink;
	else
		rulerink <<= Null;
	if(RichText::RULERSTYLE & formatinfo.paravalid)
		rulerstyle <<= formatinfo.rulerstyle;
	else
		rulerstyle <<= Null;
	tabpos.SetUnit(unit);
	if(RichText::BULLET & formatinfo.paravalid)
		bullet <<= formatinfo.bullet;
	else
		bullet <<= Null;
	if(RichText::SPACING & formatinfo.paravalid)
		linespacing <<= formatinfo.linespacing;
	else
		linespacing <<= Null;
	if(RichText::NUMBERING & formatinfo.paravalid) {
		before_number <<= formatinfo.before_number.ToWString();
		after_number <<= formatinfo.after_number.ToWString();
		reset_number <<= formatinfo.reset_number;
		for(int i = 0; i < 8; i++)
			n[i] = formatinfo.number[i];
	}
	else {
		before_number <<= Null;
		after_number <<= Null;
		reset_number <<= Null;
		reset_number.ThreeState();
		for(int i = 0; i < 8; i++)
			n[i] = Null;
	}
	tabs.Clear();
	if(RichText::TABS & formatinfo.paravalid)
		for(int i = 0; i < formatinfo.tab.GetCount(); i++)
			tabs.Add(formatinfo.tab[i].pos, formatinfo.tab[i].align, formatinfo.tab[i].fillchar);
	tabsize.Set(unit, RichText::TABSIZE & formatinfo.paravalid ? formatinfo.tabsize : Null);
	keepindent = formatinfo.indent != ComputeIndent();
	SetupIndent();
	ClearModify();
	SyncHdrFtr();
	modified = false;
}

dword WPParaFormatting::Get(RichText::FormatInfo& formatinfo)
{
	dword v = 0;
	if(!IsNull(before)) {
		formatinfo.before = ~before;
		v |= RichText::BEFORE;
	}
	if(!IsNull(lm)) {
		formatinfo.lm = ~lm;
		v |= RichText::LM;
	}
	if(!IsNull(indent)) {
		formatinfo.indent = ~indent;
		v |= RichText::INDENT;
	}
	if(!IsNull(rm)) {
		formatinfo.rm = ~rm;
		v |= RichText::RM;
	}
	if(!IsNull(after)) {
		formatinfo.after = ~after;
		v |= RichText::AFTER;
	}
	if(!IsNull(align)) {
		static int sw[] = { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT, ALIGN_JUSTIFY };
		formatinfo.align = sw[(int)~align];
		v |= RichText::ALIGN;
	}
	if(!IsNull(page)) {
		formatinfo.newpage = page;
		v |= RichText::NEWPAGE;
	}
	if(!IsNull(newhdrftr)) {
		formatinfo.newhdrftr = newhdrftr;
		v |= RichText::NEWHDRFTR;
		if(formatinfo.newhdrftr) {
			formatinfo.header_qtf = header_qtf;
			formatinfo.footer_qtf = footer_qtf;
		}
		else
			formatinfo.header_qtf = formatinfo.footer_qtf = Null;
	}
	if(!IsNull(keep)) {
		formatinfo.keep = keep;
		v |= RichText::KEEP;
	}
	if(!IsNull(keepnext)) {
		formatinfo.keepnext = keepnext;
		v |= RichText::KEEPNEXT;
	}
	if(!IsNull(orphan)) {
		formatinfo.orphan = orphan;
		v |= RichText::ORPHAN;
	}
	if(!IsNull(bullet)) {
		formatinfo.bullet = ~bullet;
		v |= RichText::BULLET;
	}
	if(!IsNull(linespacing)) {
		formatinfo.linespacing = ~linespacing;
		v |= RichText::SPACING;
	}
	if(IsNumbering()) {
		(RichPara::NumberFormat&)formatinfo = GetNumbering();
		v |= RichText::NUMBERING;
	}
	if((RichText::TABS & formatinfo.paravalid) || tabs.GetCount()) {
		formatinfo.tab.Clear();
		for(int i = 0; i < tabs.GetCount(); i++) {
			RichPara::Tab tab;
			tab.pos = tabs.Get(i, 0);
			tab.align = (int)tabs.Get(i, 1);
			tab.fillchar = (int)tabs.Get(i, 2);
			formatinfo.tab.Add(tab);
		}
		v |= RichText::TABS;
	}
	if(!IsNull(tabsize)) {
		formatinfo.tabsize = ~tabsize;
		v |= RichText::TABSIZE;
	}
	if(!IsNull(ruler)) {
		formatinfo.ruler = ~ruler;
		v |= RichText::RULER;
	}
	if(!IsNull(rulerink)) {
		formatinfo.rulerink = ~rulerink;
		v |= RichText::RULERINK;
	}
	if(!IsNull(rulerstyle)) {
		formatinfo.rulerstyle = ~rulerstyle;
		v |= RichText::RULERSTYLE;
	}
	return v;
}

struct RulerStyleDisplay : Display {
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		w.DrawRect(r, paper);
		if(!IsNull(q))
			RichPara::DrawRuler(w, r.left, (r.top + r.bottom) / 2 - 1, r.Width(), 2, ink, q);
	}
};

WPParaFormatting::WPParaFormatting()
{
	CtrlLayout(*this);
	tabtype.Add(ALIGN_LEFT, t_("Left"));
	tabtype.Add(ALIGN_RIGHT, t_("Right"));
	tabtype.Add(ALIGN_CENTER, t_("Centered"));
	tabfill.Add(0, t_("None"));
	tabfill.Add(1, t_("...."));
	tabfill.Add(2, t_("----"));
	tabfill.Add(3, t_("__"));
	tabs.AddColumn(t_("Tab position"), 2).Edit(tabpos).SetConvert(tabpos);
	tabs.AddColumn(t_("Type"), 2).Edit(tabtype).SetConvert(tabtype).InsertValue(ALIGN_LEFT);
	tabs.AddColumn(t_("Fill"), 1).Edit(tabfill).SetConvert(tabfill).InsertValue(0);
	tabs.ColumnWidths("103 89 78");
	tabs.Appending().Removing().NoAskRemove();
	tabs.WhenAcceptEdit = tabs.WhenArrayAction = THISBACK(SetMod);
	linespacing.Add(0, "1.0");
	linespacing.Add(-1, "1.5");
	linespacing.Add(-2, "2.0");
	bullet.Add(RichPara::BULLET_NONE, WPEditCtrlImg::NoneBullet());
	bullet.Add(RichPara::BULLET_ROUND, WPEditCtrlImg::RoundBullet());
	bullet.Add(RichPara::BULLET_ROUNDWHITE, WPEditCtrlImg::RoundWhiteBullet());
	bullet.Add(RichPara::BULLET_BOX, WPEditCtrlImg::BoxBullet());
	bullet.Add(RichPara::BULLET_BOXWHITE, WPEditCtrlImg::BoxWhiteBullet());
	bullet.Add(RichPara::BULLET_TEXT, WPEditCtrlImg::TextBullet());
	bullet.SetDisplay(CenteredHighlightImageDisplay());
	bullet.SetLineCy(WPEditCtrlImg::RoundBullet().GetHeight() + Zy(2));
	for(int i = 0; i < 8; i++) {
		DropList& list = n[i];
		list.Add(Null);
		list.Add(RichPara::NUMBER_NONE, " - ");
		list.Add(RichPara::NUMBER_1, "1, 2, 3");
		list.Add(RichPara::NUMBER_0, "0, 1, 2");
		list.Add(RichPara::NUMBER_a, "a, b, c");
		list.Add(RichPara::NUMBER_A, "A, B, C");
		list.Add(RichPara::NUMBER_i, "i, ii, iii");
		list.Add(RichPara::NUMBER_I, "I, II, III");
		list <<= THISBACK(SetupIndent);
	}
	before_number <<=
	after_number <<=
	reset_number <<=
	bullet <<= THISBACK(SetupIndent);
	
	newhdrftr <<= THISBACK(NewHdrFtr);
	hdrftr <<= THISBACK(EditHdrFtr);
	SyncHdrFtr();
	
	EnableNumbering();
	rulerink.NullText("---");
	rulerstyle.SetDisplay(Single<RulerStyleDisplay>());
	rulerstyle.Add(Null);
	rulerstyle.Add(RichPara::RULER_SOLID);
	rulerstyle.Add(RichPara::RULER_DOT);
	rulerstyle.Add(RichPara::RULER_DASH);
}

void WPStyleManager::EnterStyle()
{
	RichText::FormatInfo f;
	const RichStyle& s = style.Get(list.GetKey());
	f.Set(s.format);
	para.Set(unit, f);
	height <<= WPEditor::DotToPt(s.format.GetHeight());
	face <<= s.format.GetFace();
	bold = s.format.IsBold();
	italic = s.format.IsItalic();
	underline = s.format.IsUnderline();
	strikeout = s.format.IsStrikeout();
	capitals = s.format.capitals;
	ink <<= s.format.ink;
	paper <<= s.format.paper;
	next <<= s.next;
	ClearModify();
	SetupFont0();
	para.EnableNumbering();
}

void WPStyleManager::GetFont(Font& font)
{
	if(!IsNull(face))
		font.Face(~face);
	if(!IsNull(height))
		font.Height(WPEditor::PtToDot(~height));
	font.Bold(bold);
	font.Italic(italic);
	font.Underline(underline);
	font.Strikeout(strikeout);
}

void WPStyleManager::SetupFont0()
{
	Font font = Arial(120);
	GetFont(font);
	para.SetFont(font);
}

void WPStyleManager::SetupFont()
{
	SetupFont0();
	para.SetupIndent();
}

void WPStyleManager::SaveStyle()
{
	if(list.IsCursor()) {
		Uuid id = list.GetKey();
		RichStyle& s = style.Get(list.GetKey());
		if(Ctrl::IsModified() || para.IsChanged()) {
			dirty.FindAdd(id);
			RichText::FormatInfo f;
			para.Get(f);
			s.format = f;
			GetFont(s.format);
			s.format.capitals = capitals;
			s.format.ink = ~ink;
			s.format.paper = ~paper;
		}
		if(String(list.Get(1)) != s.name) {
			dirty.FindAdd(id);
			s.name = list.Get(1);
		}
		if((Uuid)~next != s.next) {
			dirty.FindAdd(id);
			s.next = ~next;
		}
	}
}

void WPStyleManager::Create()
{
	Uuid id = Uuid::Create();
	style.Add(id, style.Get(list.GetKey()));
	style.Top().next = id;
	dirty.FindAdd(id);
	list.Add(id);
	list.GoEnd();
	list.StartEdit();
}

void WPStyleManager::Remove()
{
	if(list.GetCount() > 1 && (Uuid)list.GetKey() != RichStyle::GetDefaultId()) {
		dirty.FindAdd(list.GetKey());
		style.Remove(list.GetCursor());
		list.Remove(list.GetCursor());
	}
}

void WPStyleManager::Menu(Bar& bar)
{
	bar.Add(t_("Create new style.."), THISBACK(Create))
	   .Key(K_INSERT);
	bar.Add(t_("Remove style"), THISBACK(Remove))
	   .Key(K_DELETE);
	bar.Add(t_("Rename.."), callback(&list, &ArrayCtrl::DoEdit))
	   .Key(K_CTRL_ENTER);
}

void WPStyleManager::ReloadNextStyles()
{
	next.ClearList();
	for(int i = 0; i < list.GetCount(); i++)
		next.Add(list.Get(i, 0), list.Get(i, 1));
}

void WPEditor::DisplayDefault::Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
{
	String text = q;
	w.DrawRect(r, paper);
	DrawSmartText(w, r.left, r.top, r.Width(), text, StdFont().Bold(), ink);
}

void WPStyleManager::Set(const RichText& text)
{
	list.Clear();
	int i;
	for(i = 0; i < text.GetStyleCount(); i++)
		list.Add(text.GetStyleId(i), text.GetStyle(i).name);
	list.Sort(1, WPEditor::CompareStyle);
	for(i = 0; i < text.GetStyleCount(); i++) {
		Uuid id = list.Get(i, 0);
		style.Add(id, text.GetStyle(id));
	}
	int q = list.Find(RichStyle::GetDefaultId());
	if(q >= 0)
		list.SetDisplay(q, 0, Single<WPEditor::DisplayDefault>());
	ReloadNextStyles();
}

void WPStyleManager::Set(const char *qtf)
{
	RichText txt = ParseQTF(qtf);
	Set(txt);
}

bool WPStyleManager::IsChanged() const
{
	return dirty.GetCount() || IsModified();
}

void WPStyleManager::Get(RichText& text)
{
	SaveStyle();
	for(int i = 0; i < dirty.GetCount(); i++) {
		Uuid id = dirty[i];
		int q = style.Find(id);
		if(q >= 0)
			text.SetStyle(id, style.Get(id));
		else
			text.RemoveStyle(id);
	}
}

RichText WPStyleManager::Get()
{
	RichText output;
	output.SetStyles(style);
	return output;
}

String WPStyleManager::GetQTF()
{
	return AsQTF(Get());
}

void WPStyleManager::Setup(const Vector<int>& faces, int aunit)
{
	unit = aunit;
	height.Clear();
	for(int i = 0; WPEditor::fh[i]; i++)
		height.AddList(WPEditor::fh[i]);
	face.ClearList();
	SetupFaceList(face);
	for(int i = 0; i < faces.GetCount(); i++)
		face.Add(faces[i]);
}

WPStyleManager::WPStyleManager()
{
	CtrlLayoutOKCancel(*this, t_("Styles"));
	list.NoHeader().NoGrid();
	list.AddKey();
	list.AddColumn().Edit(name);
	list.WhenEnterRow = THISBACK(EnterStyle);
	list.WhenKillCursor = THISBACK(SaveStyle);
	list.WhenBar = THISBACK(Menu);
	list.WhenAcceptEdit = THISBACK(ReloadNextStyles);
	ink.NotNull();
	face <<= height <<= italic <<= bold <<= underline <<= strikeout <<= THISBACK(SetupFont);
	Vector<int> ffs;
	Vector<int> ff;
	ff.Add(Font::ARIAL);
	ff.Add(Font::ROMAN);
	ff.Add(Font::COURIER);
	Setup(ff, WP_UNIT_DOT);
}

//LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL


double UnitMultiplier(int unit) {
	static double m[] =
	{
		1,
		72.0 / 600,
		1.0 / 600,
		25.4 / 600,
		2.54 / 600,
	};
	ASSERT(unit >= WP_UNIT_DOT && unit <= WP_UNIT_CM);
	return m[unit];
}

const char *UnitText(int unit) {
	static const char *txt[] =
	{
		"dt",
		"pt",
		"\"",
		"mm",
		"cm"
	};
	return txt[unit];
}

void WPUnitEdit::Read(double& d, int& u) const
{
	String txt = GetText().ToString();
	const char *s = txt;
	u = unit;
	d = Null;
	if(s && *s) {
		const char *e;
		int sign = 1;
		for(;;) {
			if(*s == '-' && sgn)
				sign = -1;
			else
			if(*s != ' ')
				break;
			s++;
		}
		d = ScanDouble(s, &e);
		if(IsNull(d))
			return;
		d *= sign;
		if(e == s) {
			d = Null;
			return;
		}
		while(*e == ' ') e++;
		if(*e == '\"' || *e == 'i')
			u = WP_UNIT_INCH;
		if(*e == 'm')
			u = WP_UNIT_MM;
		if(*e == 'p' || *e == 'b')
			u = WP_UNIT_POINT;
		if(*e == 'c')
			u = WP_UNIT_CM;
		if(*e == 'd')
			u = WP_UNIT_DOT;
	}
}

Value WPUnitEdit::GetData() const
{
	double q;
	int u;
	Read(q, u);
	return IsNull(q) ? Null : int(q / UnitMultiplier(u) + 0.5);
}

String WPUnitEdit::AsText(double d, int unit)
{
	if(IsNull(d))
		return Null;
	String utxt = UnitText(unit);
	if(unit == WP_UNIT_POINT)
		d = floor(10 * d + 0.5) / 10;
	return AsString(d, unit == WP_UNIT_DOT ? 0 : unit == WP_UNIT_MM ? 1 : 2) + ' ' + utxt;
}

String WPUnitEdit::DotAsText(int dot, int unit)
{
	if(IsNull(dot)) return Null;
	return AsText(dot * UnitMultiplier(unit), unit);
}

Value WPUnitEdit::Format(const Value& v) const
{
	return DotAsText(v, unit);
}

void WPUnitEdit::SetData(const Value& v)
{
	SetText(DotAsText(v, unit).ToWString());
}

bool WPUnitEdit::Key(dword key, int repcnt)
{
	if(key == K_UP)   { Spin(+1); return true; }
	if(key == K_DOWN) { Spin(-1); return true; }
	return EditField::Key(key, repcnt);
}

void WPUnitEdit::MouseWheel(Point p, int zdelta, dword keyflags)
{
	Spin(zdelta < 0 ? -1 : 1);
}

void WPUnitEdit::Spin(int delta)
{
	double q;
	int u;
	Read(q, u);
	if(IsNull(q))
		q = 0;
	else {
		double h = 10;
		switch(u) {
		case WP_UNIT_DOT:   h = 10; break;
		case WP_UNIT_POINT: h = 0.5; break;
		case WP_UNIT_MM:    h = 0.5; break;
		case WP_UNIT_CM:
		case WP_UNIT_INCH:  h = 0.05; break;
		default:         NEVER();
		}
		h *= delta;
		q = ceil(q / h + 1e-2) * h;
		if(!sgn && q < 0)
			q = 0;
	}
	SetText(AsText(q, unit).ToWString());
	UpdateAction();
}

int CharFilterUnitEdit(int c)
{
	return IsAlpha(c) ? ToLower(c) : IsDigit(c) || c == ' ' || c == '\"' || c == '.' ? c : 0;
}

int CharFilterUnitEditSgn(int c)
{
	return c == '-' ? c : CharFilterUnitEdit(c);
}

WPUnitEdit& WPUnitEdit::WithSgn(bool b)
{
	sgn = b;
	SetFilter(b ? CharFilterUnitEditSgn : CharFilterUnitEdit);
	return *this;
}

WPUnitEdit::WPUnitEdit()
{
	AddFrame(spin);
	spin.inc.WhenRepeat = spin.inc.WhenPush = THISBACK1(Spin, 1);
	spin.dec.WhenRepeat = spin.dec.WhenPush = THISBACK1(Spin, -1);
	unit = WP_UNIT_DOT;
	WithSgn(false);
}

//MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM


void WPEditor::UndoInsert::Apply(RichText& txt)
{
	txt.Remove(pos, length);
}

WPEditor::UndoRec *WPEditor::UndoInsert::GetRedo(const RichText& txt)
{
	return new UndoRemove(txt, pos, length);
}

WPEditor::UndoInsert::UndoInsert(int pos, int length, bool typing)
: pos(pos), length(length), typing(typing) {}

// -----------------------

void WPEditor::UndoRemove::Apply(RichText& txt)
{
	txt.Insert(pos, text);
}

WPEditor::UndoRec *WPEditor::UndoRemove::GetRedo(const RichText& txt)
{
	return new UndoInsert(pos, text.GetLength());
}

WPEditor::UndoRemove::UndoRemove(const RichText& txt, int pos, int length)
: pos(pos)
{
	text = txt.Copy(pos, length);
}

// -----------------------

void WPEditor::UndoFormat::Apply(RichText& txt)
{
	txt.RestoreFormat(pos, format);
}

WPEditor::UndoRec *WPEditor::UndoFormat::GetRedo(const RichText& txt)
{
	return new UndoFormat(txt, pos, length);
}

WPEditor::UndoFormat::UndoFormat(const RichText& txt, int pos, int length)
: pos(pos), length(length)
{
	format = txt.SaveFormat(pos, length);
}

// -----------------------

void WPEditor::UndoStyle::Apply(RichText& txt)
{
	txt.SetStyle(id, style);
}

WPEditor::UndoRec *WPEditor::UndoStyle::GetRedo(const RichText& txt)
{
	return new UndoStyle(txt, id);
}

WPEditor::UndoStyle::UndoStyle(const RichText& txt, const Uuid& id)
: id(id)
{
	style = txt.GetStyle(id);
}

// -----------------------

void WPEditor::UndoStyles::Apply(RichText& txt)
{
	txt.SetStyles(styles);
}

WPEditor::UndoRec *WPEditor::UndoStyles::GetRedo(const RichText& txt)
{
	return new UndoStyles(txt);
}

WPEditor::UndoStyles::UndoStyles(const RichText& txt)
{
	styles <<= txt.GetStyles();
}

//NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN


void WPEditor::UndoTableFormat::Apply(RichText& txt)
{
	txt.SetTableFormat(table, format);
}

WPEditor::UndoRec *WPEditor::UndoTableFormat::GetRedo(const RichText& txt)
{
	return new UndoTableFormat(txt, table);
}

WPEditor::UndoTableFormat::UndoTableFormat(const RichText& txt, int table)
: table(table)
{
	format = txt.GetTableFormat(table);
}

// -----------------------


void WPEditor::UndoCreateTable::Apply(RichText& txt)
{
	txt.DestroyTable(table);
}

WPEditor::UndoRec *WPEditor::UndoCreateTable::GetRedo(const RichText& txt)
{
	return new UndoDestroyTable(txt, table);
}

// -----------------------

void WPEditor::UndoDestroyTable::Apply(RichText& txt)
{
	txt.SetTable(pos, table);
}

WPEditor::UndoRec *WPEditor::UndoDestroyTable::GetRedo(const RichText& txt)
{
	return new UndoCreateTable(txt.GetRichPos(pos).table + 1);
}

WPEditor::UndoDestroyTable::UndoDestroyTable(const RichText& txt, int tab)
{
	pos = txt.GetCellPos(tab, 0, 0).pos;
	table = txt.CopyTable(tab);
}

// -----------------------

void WPEditor::UndoInsertParaSpecial::Apply(RichText& txt)
{
	txt.RemoveParaSpecial(table, before);
	RichCellPos p = txt.GetCellPos(table, 0, 0);
}

WPEditor::UndoRec *WPEditor::UndoInsertParaSpecial::GetRedo(const RichText& txt)
{
	return new UndoRemoveParaSpecial(txt, table, before);
}

// -----------------------

void WPEditor::UndoRemoveParaSpecial::Apply(RichText& txt)
{
	txt.InsertParaSpecial(table, before, format);
}

WPEditor::UndoRec *WPEditor::UndoRemoveParaSpecial::GetRedo(const RichText& txt)
{
	return new UndoInsertParaSpecial(table, before);
}

WPEditor::UndoRemoveParaSpecial::UndoRemoveParaSpecial(const RichText& txt, int table, bool before)
: table(table), before(before)
{
	RichCellPos p = txt.GetCellPos(table, 0, 0);
	format = txt.GetRichPos(before ? p.pos - 1 : p.pos + p.tablen + 1).format;
}

// -----------------------

void WPEditor::UndoTable::Apply(RichText& txt)
{
	txt.ReplaceTable(table, copy);
}

WPEditor::UndoRec *WPEditor::UndoTable::GetRedo(const RichText& txt)
{
	return new UndoTable(txt, table);
}

WPEditor::UndoTable::UndoTable(const RichText& txt, int tab)
{
	table = tab;
	copy = txt.CopyTable(tab);
}

//OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO


bool HasNumbering(const RichPara::Format& f)
{
	if(f.after_number.GetCount() || f.before_number.GetCount())
		return true;
	for(int i = 0; i < 8; i++)
		if(f.number[i] != RichPara::NUMBER_NONE)
			return true;
	return false;
}

bool WPEditor::RemoveBullet(bool backspace)
{
	RichPos p = text.GetRichPos(cursor);
	if((backspace ? p.posinpara : p.paralen) == 0 &&
	   (p.format.bullet != RichPara::BULLET_NONE || HasNumbering(p.format))) {
	    Style();
		RichText::FormatInfo nobullet;
		nobullet.paravalid = RichText::NUMBERING|RichText::BULLET;
		nobullet.charvalid = 0;
		ApplyFormatInfo(nobullet);
		return true;
	}
	return false;
}

bool WPEditor::Key(dword key, int count)
{
	useraction = true;
	NextUndo();
	if(CursorKey(key, count))
		return true;
	if(IsReadOnly())
		return false;
	switch(key) {
	case K_CTRL_BACKSPACE:
		if(RemoveSelection(true)) return true;
		if(cursor > 0 && IsW(text[cursor - 1])) {
			int c = cursor;
			ReadFormat();
			MoveWordLeft(false);
			if(InvalidRange(cursor, c))
				return true;
			Remove(cursor, c - cursor);
			objectpos = -1;
			FinishNF();
			return true;
		}
	case K_BACKSPACE:
	case K_SHIFT_BACKSPACE:
		if(RemoveSelection(true)) return true;
		if(RemoveBullet(true)) break;
		if(cursor <= 0 || RemoveSpecial(cursor, cursor - 1, true))
			return true;
		anchor = --cursor;
		begtabsel = false;
		if(cursor > 0) {
			RichPos p = text.GetRichPos(cursor - 1);
			if(p.format.bullet != RichPara::BULLET_NONE || HasNumbering(p.format)) {
				Remove(cursor, 1, true);
				break;
			}
		}
		Remove(cursor, 1);
		break;
	case K_DELETE:
		if(RemoveSelection()) return true;
		if(cursor < text.GetLength() && !RemoveSpecial(cursor, cursor + 1, false))
			Remove(cursor, 1, true);
		break;
	case K_INSERT:
		overwrite = !overwrite;
		PlaceCaret();
		break;
	case K_CTRL_DELETE:
		if(RemoveSelection()) return true;
		if(cursor < text.GetLength()) {
			int c = cursor;
			if(IsW(text[c]))
				MoveWordRight(false);
			else
				cursor++;
			if(InvalidRange(cursor, c))
				return true;
			Remove(c, cursor - c);
			cursor = anchor = c;
			begtabsel = false;
			break;
		}
		break;
	case K_CTRL_Z:
		Undo();
		return true;
	case K_SHIFT_CTRL_Z:
		Redo();
		return true;
	case K_ENTER: {
			if(singleline)
				return false;
			if(!RemoveSelection() && InsertLineSpecial())
				return true;
			if(RemoveBullet(false))
				break;
			RichText::FormatInfo f = formatinfo;
			InsertLine();
			formatinfo = f;
			ShowFormat();
			FinishNF();
		}
		return true;
	case K_CTRL_ENTER:
		{
			int c = GetCursor(), l = GetLength();
			RichObject object;
			while(c < l) {
				RichPos p = text.GetRichPos(c);
				object = p.object;
				if(object || p.chr > ' ')
					break;
				c++;
			}
			if(object) {
				NextUndo();
				objectpos = c;
				RichObject o = object;
				o.DefaultAction(context);
				if(o.GetSerialId() != object.GetSerialId())
					ReplaceObject(o);
				return true;
			}
		}
		return false;
	case K_F9:
		EvaluateFields();
		break;
	case K_F3:
		Find();
		break;
	case K_CTRL_H:
		Hyperlink();
		break;
	case K_CTRL_Q:
		IndexEntry();
		break;
	case K_ESCAPE:
		CloseFindReplace();
		return false;
	case K_CTRL_C:
	case K_CTRL_INSERT:
		Copy();
		return true;
	case K_CTRL_X:
	case K_SHIFT_DELETE:
		Cut();
		return true;
	case K_CTRL_V:
	case K_SHIFT_INSERT:
		Paste();
		return true;
	case K_SHIFT_CTRL_SPACE:
	case K_CTRL_SPACE:
		key = 160;
	case K_TAB:
		if(cursorp.table && cursorp.posintab == cursorp.tablen) {
			TableInsertRow();
			return true;
		}
		if(cursorp.table && cursorp.posincell == cursorp.celllen) {
			cursor = anchor = cursor + 1;
			begtabsel = false;
			break;
		}
	default:
		if(key >= K_ALT_0 && key <= K_ALT_9) {
			ApplyStyleKey(key - K_ALT_0);
			return true;
		}
		if(key >= (K_SHIFT|K_ALT_0) && key <= (K_SHIFT|K_ALT_9)) {
			ApplyStyleKey(key - (K_SHIFT|K_ALT_0) + 10);
			return true;
		}
		if(key == K_SHIFT_SPACE)
			key = ' ';
		if(key == 9 || key >= 32 && key < 65536) {
			RichPara::Format f;
			if(IsSelection()) {
				f = text.GetRichPos(min(cursor, anchor)).format;
				RemoveSelection();
			}
			else
				f = formatinfo;
			RichPara p;
			p.format = f;
			p.Cat(WString(key, count), f);
			RichText txt;
			txt.SetStyles(text.GetStyles());
			txt.Cat(p);
			if(overwrite) {
				RichPos p = text.GetRichPos(cursor);
				if(p.posinpara < p.paralen)
					Remove(cursor, 1);
			}
			Filter(txt);
			Insert(cursor, txt, true);
			Move(cursor + count, false);
			break;
		}
		return false;
	}
	objectpos = -1;
	Finish();
	return true;
}

