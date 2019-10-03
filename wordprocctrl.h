#ifndef _wordprocctrl_h_
#define _wordprocctrl_h_


#include "wpeditor.h"

#include <PdfDraw/PdfDraw.h>
using namespace Upp;

#include <string>


struct WordProcCtrl : public Ctrl
{
	using CLASSNAME=WordProcCtrl;

	virtual void DragAndDrop(Point, PasteClip& d);
	virtual void FrameDragAndDrop(Point, PasteClip& d);

	WPEditor editor;
	MenuBar menubar;
	ToolBar toolbar;
	
	std::string itemname;
	size_t itemid; //db-id for modeler:journal
	
	static LRUList& lrufile() { static LRUList l; return l; }

	Callback WhenSave;
	
	virtual void GotFocus();
	virtual void LostFocus();
	
	void FillFonts();

	void RPopup(Bar &bar);
	
	void Load(const String& filename);
	void OpenFile(const String& fn);
	void Open();
	void Save0(String fn);
	void Save();
	void SaveAs();
	void Print();
	void Pdf();
	void About();
	void SetBar();
	void FileBar(Bar& bar);
	void AboutMenu(Bar& bar);
	void MainMenu(Bar& bar);
	void MainBar(Bar& bar);
	void Tools(Bar& bar);

	void initwordproc();
	
	WordProcCtrl();
	virtual ~WordProcCtrl();
};

#endif
