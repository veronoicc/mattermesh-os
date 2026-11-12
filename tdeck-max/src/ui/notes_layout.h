#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "theme.h"
#include "services/notes.h"
// Notes app (NotesSection): the list, one note (checklist items tick on a tap), the editor, rename, the options sheet and the
// delete dialog. The view is filled by notes_app.inc, drawn by ui/notes_renderer.inc.
namespace mm {
enum class NotesMode:uint8_t {List,View,Edit,Rename,Options,Confirm};// Rename is also the title prompt of a new checklist; Confirm: "delete this note?" over the note
struct NoteRow {std::string title,preview,progress,date;bool pinned=false;};
struct NotesView {
 NotesMode mode=NotesMode::List;bool sd=true;
 // List: focus 0 New note, 1 Options, 2+i row i; scroll = first row shown; total = notes on the card
 std::vector<NoteRow> rows;int focus=0,scroll=0,total=0;
 // Note: header (title, meta line), the lines of the text (line 0 is the title), first row shown, the checkbox line / action focused (-1: none), checked items
 std::string title,meta;bool pinned=false,showChecked=true;std::vector<mm_notes::Line> lines;int viewScroll=0,focusLine=-1,focusAction=-1,checked=0;
 // Editor / rename: the text, the rename heading and the focused button (-1: none)
 std::string text,heading;int editFocus=-1;
 // Options: sort 0 Modified / 1 Title, new note 0 Text / 1 Checklist (showChecked above), the focused row
 int sort=0,newType=0,optFocus=0;
};
// List: the New note card with the options button beside it, five note rows, the count line.
constexpr int NoteRowH=38,NoteRows=5,NoteListY=Top+54,NotesCountY=Bottom-13;
constexpr Rect NotesOptions={W-Margin-44,Top+4,44,44},NotesNew={Margin,Top+4,W-2*Margin-44-Gap,44};
inline Rect noteRowRect(int i){return {Margin,NoteListY+i*NoteRowH,W-2*Margin,NoteRowH-2};}
// Note view: header card, the text (NoteViewRows rows of NoteLineH), then EDIT ADD CLEAR / RENAME PIN DELETE.
constexpr int NoteLineH=17,NoteViewRows=8,NoteCheckW=20,NoteActionCount=6;
constexpr Rect NoteTitle={Margin,Top+4,W-2*Margin,36},NoteBody={Margin,Top+44,W-2*Margin,NoteViewRows*NoteLineH+10};
inline Rect noteBodyRow(int i){return {NoteBody.x+4,NoteBody.y+5+i*NoteLineH,NoteBody.w-8,NoteLineH};}
inline Rect noteActionRect(int i){int w=(W-2*Margin-2*Gap)/3;return {Margin+(i%3)*(w+Gap),Bottom-66+(i/3)*36,w,30};}
enum NoteAction {NoteEdit,NoteAdd,NoteClear,NoteRename,NotePin,NoteDelete};
constexpr int NoteTextW=W-2*Margin-8-14;// wrap width of a note line (the scroll arrows take the right 14 px); items are NoteCheckW narrower
// Editor: the text box, the byte counter, DONE and a second button (ADD ITEM; CANCEL while renaming). Rename: one line, heading above.
constexpr Rect NoteEditBox={Margin,Top+4,W-2*Margin,Bottom-52-(Top+4)};
constexpr int NoteEditCounterY=Bottom-48,NoteRenameLabelY=Top+4;
constexpr Rect NoteEditDone={Margin,Bottom-32,(W-2*Margin-Gap)/2,30},NoteEditSide={Margin+(W-2*Margin-Gap)/2+Gap,Bottom-32,(W-2*Margin-Gap)/2,30};
constexpr Rect NoteRenameBox={Margin,Top+24,W-2*Margin,34};
// Options sheet: three rows (sort, show checked items, new note type).
constexpr int NoteOptionCount=3,NoteOptionsLabelY=Top+4;
inline Rect noteOptionRect(int i){return {Margin,Top+24+i*38,W-2*Margin,34};}
constexpr Rect NotesDeleteYes=ChatClearYes,NotesDeleteNo=ChatClearNo;
// List focus: New note 0, Options 1, then the rows.
inline int noteListMove(int focus,int dir,int rows){return std::max(0,std::min(rows+1,focus+dir));}
// Note focus: 0..items-1 the checkboxes, then the six actions.
inline int noteViewStep(int focus,int dir,int items){return std::max(0,std::min(items+NoteActionCount-1,focus+dir));}
// L / H: next / previous action, entering the actions from the checkboxes.
inline int noteViewAction(int focus,int dir,int items){if(dir>0)return focus<items?items:std::min(items+NoteActionCount-1,focus+1);return focus>items?focus-1:focus;}
}
