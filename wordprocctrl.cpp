
#include "wordprocctrl.h"
#include <utilfuncs/utilfuncs.h>

#define IMAGECLASS UWordImg
#define IMAGEFILE <WordProcCtrl/wordprocctrl.iml>
#include <Draw/iml.h>

FileSel& WPFS()
{
	static FileSel fs;
	return fs;
}

FileSel& PdfFs()
{
	static FileSel fs;
	return fs;
}

void WordProcCtrl::GotFocus()
{
	editor.SetFocus();
}

void WordProcCtrl::LostFocus() { if (WhenSave) WhenSave(); }

void WordProcCtrl::FileBar(Bar& bar)
{
	bar.Add("Insert from file", CtrlImg::open(), THISBACK(Open));
	//bar.ToolGap();
	bar.MenuSeparator();
	bar.Add("Print..", CtrlImg::print(), THISBACK(Print));
	bar.Add("Export to PDF..", UWordImg::pdf(), THISBACK(Pdf));
	//bar.ToolGap();
	bar.MenuSeparator();
	bar.Add("Save copy to file", CtrlImg::save_as(), THISBACK(SaveAs));
	//bar.ToolGap();
	bar.MenuSeparator();
	bar.Add(editor.IsModified(), "Save", UWordImg::modelersave(), THISBACK(Save)).Key(K_CTRL_S).Help("Save current document to modeler's database");
}

void WordProcCtrl::AboutMenu(Bar& bar)
{
	bar.Add("About..", THISBACK(About));
}

void WordProcCtrl::MainMenu(Bar& bar)
{
	bar.Add("File", THISBACK(FileBar));
	bar.Add("Help", THISBACK(AboutMenu));
}

//void WordProcCtrl::New() { new WordProcCtrl; }

bool IsRTF(const char *fn) { return ToLower(GetFileExt(fn)) == ".rtf"; }
bool IsTEXT(const char *fn) { return ToLower(GetFileExt(fn)) == ".txt"; }
bool IsQTF(const char *fn) { return ToLower(GetFileExt(fn)) == ".qtf"; }

///ugly hack... changed to act as paste-from-file...
void WordProcCtrl::Load(const String& name)
{
	//lrufile().NewEntry(name);
	if(IsRTF(name))
	{
		//editor.Pick(ParseRTF(LoadFile(name)));
		editor.PasteText(ParseRTF(LoadFile(name)));
	}
	//else editor.SetQTF(LoadFile(name));
	else
	{
		RichTextCtrl rtc;
		rtc.SetZoom(editor.GetZoom());
		
		String S=LoadFile(name);
		if (IsTEXT(name))
		{
			S.Insert(0,"\n");
			rtc.SetData(DeQtf(S));
		}
		else
		{
			S.Insert(0,"&");
			rtc.SetQTF(S);
		}
		editor.PasteText(rtc.Get());
	}
	editor.SetModify();
}

void WordProcCtrl::OpenFile(const String& fn) { Load(fn); }

void WordProcCtrl::Open()
{
	FileSel& fs=WPFS();
	if (fs.ExecuteOpen()) OpenFile(fs);
}

void WordProcCtrl::DragAndDrop(Point, PasteClip& d)
{
	if (IsAvailableFiles(d))
	{
		Vector<String> fn = GetFiles(d);
		for (int open=0; open<2; open++)
		{
			for (int i=0; i<fn.GetCount(); i++)
			{
				if (FileExists(fn[i]) && (IsRTF(fn[i]) || IsTEXT(fn[i]) || IsQTF(fn[i])))
				{
					if (open) OpenFile(fn[i]);
					else { if (d.Accept()) break; return; }
				}
			}
			if (!d.IsAccepted()) return;
		}
	}
}

void WordProcCtrl::FrameDragAndDrop(Point p, PasteClip& d) { DragAndDrop(p, d); }

void WordProcCtrl::Save0(String fn)
{
	//lrufile().NewEntry(fn);
	if (fn.IsEmpty()) SaveAs();
	else
	{
		if (SaveFile(fn, IsRTF(fn)	? EncodeRTF(editor.Get())
									: IsTEXT(fn)	? ToUtf8(editor.Get().GetPlainText())
													: editor.GetQTF()))
		{
			//ClearModify(); only when saved to db
		}
		else Exclamation("Error saving the file [* " + DeQtf(fn) + "]!");
	}
}

void WordProcCtrl::Save() //to db
{
	if (!editor.IsModified()) return;
	if (WhenSave) WhenSave();
	ClearModify();
}

void WordProcCtrl::SaveAs()
{
	FileSel &fs=WPFS();
	if (fs.ExecuteSaveAs()) { String fn=fs; Save0(fn); }
}

void WordProcCtrl::Print() { editor.Print(); }

void WordProcCtrl::Pdf()
{
	FileSel &fs=PdfFs();
	if (!fs.ExecuteSaveAs("Output PDF file")) return;
	Size page=Size(3968, 6074);
	PdfDraw pdf;
	UPP::Print(pdf, editor.Get(), page);
	SaveFile(~fs, pdf.Finish());
}

void WordProcCtrl::About()
{
	PromptOK("[A5 uWord]&Using [*^www://upp.sf.net^ Ultimate`+`+] technology.");
}

void WordProcCtrl::Tools(Bar& bar)
{
	
	editor.EditTools(bar);
	bar.Gap();
	editor.SpellCheckTool(bar);
	bar.Gap();
	editor.FontTools(bar);
	//bar.Gap();
	editor.InkTool(bar); //fg
	editor.PaperTool(bar); //bg
	bar.Gap();
	editor.LanguageTool(bar);
	bar.Break();
	editor.StyleTool(bar);
	bar.Gap();
	editor.ParaTools(bar);
	bar.Gap();
	editor.HeaderFooterTool(bar);
	bar.Gap();
	editor.TableTools(bar);
	bar.Gap();
	editor.HyperlinkTool(bar, INT_MAX);
}

void WordProcCtrl::MainBar(Bar& bar)
{
	FileBar(bar);
	bar.Separator();
	//editor.DefaultBar(bar);
	Tools(bar);
	
}

void WordProcCtrl::SetBar()
{
	toolbar.Set(THISBACK(MainBar));
}

void WordProcCtrl::initwordproc()
{
	SetLanguage(LNG_ENGLISH);
	SetDefaultCharset(CHARSET_UTF8);
	WPFS().Type("QTF files", "*.qtf").Type("RTF files", "*.rtf").Type("Text files", "*.txt").AllFilesType().DefaultExt("qtf");
	PdfFs().Type("PDF files", "*.pdf").AllFilesType().DefaultExt("pdf");
}

void WordProcCtrl::RPopup(Bar &bar)
{
	bar.Separator();
	editor.FindReplaceTool(bar);
	editor.UndoTool(bar);
	editor.RedoTool(bar);
}

WordProcCtrl::WordProcCtrl()
{
	initwordproc();
	
	AddFrame(menubar);
	AddFrame(TopSeparatorFrame());
	AddFrame(toolbar);
	Add(editor.SizePos());
	menubar.Set(THISBACK(MainMenu));

	itemname="New Item";
	itemid=0;

	editor.ClearModify();
	SetBar();

	editor.WhenBar << THISBACK(RPopup); // '<<' appends rpopup, '=' will replace
	
	editor.WhenRefreshBar = THISBACK(SetBar);
	

	FillFonts();
	
	editor.NoRuler();
	editor.ShowCodes(Null);
	editor.FixedLang(0);
	editor.SpellCheck(true);

	editor.SetFocus();
	
}

WordProcCtrl::~WordProcCtrl() { }

void WordProcCtrl::FillFonts()
{
	//todo
	
	//telluser(" FACE-COUNT = ", Font::GetFaceCount());
	auto lt=[](const String &l, const String &r)->bool{ return (ToLower(l)<ToLower(r)); };
	std::map<String, int, decltype(lt) > fi(lt);
	Vector<int> VF;
	for (int i=0;i<Font::GetFaceCount();i++) { fi[Font::GetFaceName(i)]=i; }
	for (auto p:fi) VF.push_back(p.second);
	
	editor.FontFaces(VF);
	
	//SetupFaceList(toolbar . FontTools
}

