/*
 * RTF-to-LaTeX2e translation writer code.
 * (c) 1999 Ujwal S. Sathyam
 * (c) 2011 Scott Prahl
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include <unistd.h>

# include "rtf.h"
# include "tokenscan.h"
# include "cole.h"
# include "rtf2latex2e.h"
# include "eqn.h"
# include "init.h"

void __cole_dump(void *_m, void *_start, uint32_t length, char *msg);

# define  MAX_BLANK_LINES       2
# define  MATH_NONE_MODE        0
# define  MATH_INLINE_MODE      1
# define  MATH_DISPLAY_MODE     2

# define  PREVIOUS_COLUMN_VALUE -10000

void RTFSetOutputStream(FILE * stream);
static void ReadObject(void);

int braceLevel = 0;            /* a counter for keeping track of opening and closing braces */
extern FILE *ifp, *ofp;

# define EQUATION_OFFSET 35
# define MTEF_HEADER_SIZE 28

int UsedColor[256];
const char *objectClassList[] = {
    "Unknown",
    "Equation",
    "Word.Picture",
    "MSGraph.Chart",
    NULL
};

const char *justificationList[] = {
    "\\raggedright",
    "\\centering",
    "\\raggedleft"
};

const char *environmentList[] = {
    "flushleft",
    "center",
    "flushright"
};

const char *fontSizeList[] = {
    "\\tiny",
    "\\scriptsize",
    "\\footnotesize",
    "\\small",
    "\\normalsize",
    "\\large",
    "\\Large",
    "\\LARGE",
    "\\huge",
    "\\Huge"
};

static struct {
    boolean newStyle;
    int cols;
} section;

static struct {
    int class;
    char className[rtfBufSiz];
    int word97;
} object;

int wrapCount = 0;
int word97ObjectType;
boolean nowBetweenParagraphs;
boolean suppressLineBreak;
boolean requireSetspacePackage;
boolean requireTablePackage;
boolean requireMultirowPackage;
boolean requireGraphicxPackage;
boolean requireAmsSymbPackage;
boolean requireMultiColPackage;
boolean requireUlemPackage;
boolean requireFixLtx2ePackage;
boolean requireHyperrefPackage;
boolean requireAmsMathPackage;
boolean requireFancyHdrPackage;
size_t packagePos;
size_t beginDocumentPos;
boolean insideTable;
boolean insideFootnote;
boolean insideHyperlink;
boolean insideHeaderFooter;

char *preambleFancyHeader;
char *preambleFancyHeaderFirst;

#ifdef PICT2PDF
char * WritePictAsPDF(char *pict);
#endif

char *outMap[rtfSC_MaxChar];

FILE *ostream;
parStyleStruct paragraph, paragraphWritten;
textStyleStruct textStyle, textStyleWritten;
int current_vspace;
pictureStruct picture;
equationStruct oleEquation;
tableStruct table;

int g_debug_par_start       = 0;
int g_debug_table_prescan   = 0;
int g_debug_table_writing   = 0;
int g_debug_char_style      = 0;

char *UnicodeSymbolFontToLatex[];
char *UnicodeGreekToLatex[];

/*
 * a useful diagnostic function to examine the token just read.
 */
void ExamineToken(char * tag)
{
    printf("********** %s **********\n", tag);
    printf("* Token is %s\n", rtfTextBuf);
    printf("* Class is %3d", rtfClass);
    switch (rtfClass) {
    case rtfUnknown: printf(" (rtfUnknown)\n"); break;
    case rtfGroup: printf(" (rtfGroup)\n"); break;
    case rtfText: printf(" (rtfText)\n"); break;
    case rtfControl: printf(" (rtfControl)\n"); break;
    case rtfEOF: printf(" (rtfEOF)\n"); break;
    default: printf(" (not one of the basic five)\n"); break;
    }

    printf("* Major is %3d", rtfMajor);
    if (rtfClass == rtfText) {
        printf(" raw='%c' \n", rtfMajor);
    } else if (rtfClass == rtfGroup) {
        printf(" (%s)\n", rtfMajor? "rtfEndGroup" : "rtfBeginGroup");
    } else {
        switch (rtfMajor) {
            case rtfVersion: printf(" (rtfVersion)\n"); break;
            case rtfDefFont: printf(" (rtfDefFont)\n"); break;
            case rtfCharSet: printf(" (rtfCharSet)\n"); break;
            case rtfDestination: printf(" (rtfDestination)\n"); break;
            case rtfFontFamily: printf(" (rtfFontFamily)\n"); break;
            case rtfFontAttr: printf(" (rtfFontAttr)\n"); break;
            case rtfColorName: printf(" (rtfColorName)\n"); break;
            case rtfFileAttr: printf(" (rtfFileAttr)\n"); break;
            case rtfFileSource: printf(" (rtfFileSource)\n"); break;
            case rtfStyleAttr: printf(" (rtfStyleAttr)\n"); break;
            case rtfKeyCodeAttr: printf(" (rtfKeyCodeAttr)\n"); break;
            case rtfDocAttr: printf(" (rtfDocAttr)\n"); break;
            case rtfSectAttr: printf(" (rtfSectAttr)\n"); break;
            case rtfParAttr: printf(" (rtfParAttr)\n"); break;
            case rtfPosAttr: printf(" (rtfPosAttr)\n"); break;
            case rtfTblAttr: printf(" (rtfTblAttr)\n"); break;
            case rtfCharAttr: printf(" (rtfCharAttr)\n"); break;
            case rtfACharAttr: printf(" (rtfACharAttr)\n"); break;
            case rtfSpecialChar: printf(" (rtfSpecialChar)\n"); break;
            case rtfBookmarkAttr: printf(" (rtfBookmarkAttr)\n"); break;
            case rtfPictAttr: printf(" (rtfPictAttr)\n"); break;
            case rtfObjAttr: printf(" (rtfObjAttr)\n"); break;
            case rtfDrawAttr: printf(" (rtfDrawAttr)\n"); break;
            case rtfFNoteAttr: printf(" (rtfFNoteAttr)\n"); break;
            case rtfFieldAttr: printf(" (rtfFieldAttr)\n"); break;
            case rtfIndexAttr: printf(" (rtfIndexAttr)\n"); break;
            case rtfTOCAttr: printf(" (rtfTOCAttr)\n"); break;
            case rtfNeXTGrAttr: printf(" (rtfNeXTGrAttr)\n"); break;
            case rtfWord97ObjAttr: printf(" (rtfWord97ObjAttr)\n"); break;
            case rtfAnsiCharAttr: printf(" (rtfAnsiCharAttr)\n"); break;
            default: printf(" (unknown)\n"); break;
        }
    }

    printf("* Minor is %3d", rtfMinor);
    if (rtfClass == rtfText) {
        printf(" std=0x%2x\n", rtfMinor);
    } else {
        printf("\n");
    }
    printf("* Param is %3d\n\n", (int) rtfParam);
}

static void PutIntAsUtf8(int x)
{
    x &= 0x0000FFFF;
    if (x < 0x80){
        fputc((char) x, ostream);
        wrapCount++;
        return;
    }

    if (x < 0xA0) {
        fprintf(stderr, "there should be no such character c='%c'=0x%02x\n",(char) x,x);
        return;
    }

    if (x<0x07FF) {
        unsigned char d, e;
        d = 0xC0 + (x & 0x07C0)/64;
        e = 0x80 + (x & 0x003F);
        fputc(d, ostream);
        fputc(e, ostream);
        wrapCount+=2;
        return;
    }

    if (x<0xFFFF) {
        unsigned char c, d, e;
        c = 0xE0 + (x & 0xF000)/4096;
        d = 0x80 + (x & 0x0FC0)/64;
        e = 0x80 + (x & 0x003F);
        fputc(c, ostream);
        fputc(d, ostream);
        fputc(e, ostream);
        wrapCount+=3;
        return;
    }

}

/* 
 * Some environments fail if there is a blank line in
 * the argument ... e.g., \section{} will set suppressLineBreak
 * so, in this case, make sure that there is at least one char
 * on each line before writing a '\n' to the latex file.
 * We can't use wrapCount for this purpose because an "empty"
 * line of spaces will also cause these environments grief.
 *
 * Furthermore, make sure that at most two '\n' are output
 * to the latex file at a time just for esthetic reasons.
 *
 * Finally, reset wrapCount to zero every time a '\n' is 
 * written to the LaTeX file so that WrapText() below can
 * work properly
 */
static void PutLitChar(int c)
{
    static int lf_in_succession = 0;
    static int no_chars_in_row = true;

    if (c != '\n') {
        lf_in_succession = 0;
        if (c != ' ') no_chars_in_row=false;
        PutIntAsUtf8(c & 0x00ff);
        return;
    }

    if (suppressLineBreak && no_chars_in_row) {
        lf_in_succession = 0;
        PutIntAsUtf8((int) ' ');
        return;
    }

    lf_in_succession++;
    no_chars_in_row = true;
    if (lf_in_succession > 2)
        return;

    wrapCount = 0;

    PutIntAsUtf8(c & 0x00ff);
}

/* copy a sequence of bytes to the ostream, avoiding all the encoding machinery */
static void PutLitByteStr(char *s)
{
    char *p = s;
    if (!s) return;
    while (*p) {
        fputc(*p, ostream);
        p++;
    }
}

static void PutLitStr(char *s)
{
    char *p = s;
    if (!s) return;
    while (*p) {
        PutLitChar(*p);
        p++;
    }
}

static void PutMathLitStr(char *s)
{
    if (s && *s) {
        PutLitStr("$");
        PutLitStr(s);
        PutLitStr("$");
    }
}

static void InsertNewLine(void)
{
    PutLitChar('\n');
}

/* useful for just printing filenames in latex doc */
static void PutEscapedLitStr(char *s)
{
    int i=0;
    while (s[i]) {
        switch (s[i]) {
        case '_':
            PutLitStr("\\_");
            break;
        case '%':
            PutLitStr("\\%");
            break;
        case '\\':
            PutLitStr("\\textbackslash{}");
            break;
        default:
            PutLitChar(s[i]);
        }
        i++;
    }
}

/*
 * This function reads colors from the color table and defines them in
 * LaTeX format to be included in the preamble.
 * This is done after the color table has been read (see above).
 */
static void DefineColors(int ignoreUsedColors)
{
    RTFColor *rtfColorPtr;
    int i = 1;
    float Red, Blue, Green;
    char buf[rtfBufSiz];

    while ((rtfColorPtr = RTFGetColor(i)) != NULL) {
        Red = rtfColorPtr->rtfCRed / 255.0;
        Green = rtfColorPtr->rtfCGreen / 255.0;
        Blue = rtfColorPtr->rtfCBlue / 255.0;
        
        if (ignoreUsedColors || (!ignoreUsedColors && UsedColor[i])) {
            snprintf(buf, rtfBufSiz, "\\definecolor{color%02d}{rgb}",i);
            PutLitStr(buf);
            snprintf(buf, rtfBufSiz, "{%1.2f,%1.2f,%1.2f}\n", Red, Green, Blue);
            PutLitStr(buf);
        }

        i++;
    }
}

static void WriteColors(void)
{
    ReadColorTbl();
    if (!prefs[pConvertTextColor]) return;
    DefineColors(true);
}

/*
 * Eventually this should keep track of the destination of the
 * current state and only write text when in the initial state.
 *
 * If the output sequence is unspecified in the output map, write
 * the character's standard name instead.  This makes map deficiencies
 * obvious and provides incentive to fix it. :-)
 */

static void PutStdChar(int stdCode)
{
    char *oStr = NULL;
    char buf[rtfBufSiz];

    if (stdCode == rtfSC_nothing) {
        RTFMsg("* Unknown character %c (0x%x)!\n", rtfTextBuf[0], rtfTextBuf[0]);
        ExamineToken("PutStdChar");
        PutLitStr("(unknown char)");
        return;
    }
    oStr = outMap[stdCode];
    if (!oStr) {        /* no output sequence in map */
        snprintf(buf, rtfBufSiz, "(%s)", RTFStdCharName(stdCode));
        oStr = buf;
        PutLitStr(oStr);
        return;
    }

    if (oStr[0] == '0' && (oStr[1] == 'x' || oStr[1] == 'X')) {
        int x = RTFHexStrToInt(oStr);
        /*fprintf(stderr,"hex string = '%s' = %d\n",oStr,x);*/
        PutIntAsUtf8(x);
        return;
    }

    PutLitStr(oStr);
}

/*
 * make sure we write this all important stuff. This routine is called
 * whenever something is written to the output file.
 */
static int wroteBeginDocument = false;
static int CheckForBeginDocument(void)
{
    char buf[100];
    
    if (insideHeaderFooter) return 0;
    
    if (!wroteBeginDocument) {

        if (preambleOurDefs) free(preambleOurDefs);
        preambleOurDefs = malloc(1000);
        preambleOurDefs[0] = '\0';
        
        if (prefs[pConvertPageSize]) {
            snprintf(buf, 100, "\\setlength{\\oddsidemargin}{%dpt}\n", 72 - prefs[pPageLeft]/20);
            strcat(preambleOurDefs,buf);
            snprintf(buf, 100, "\\setlength{\\evensidemargin}{%dpt}\n", 72 - prefs[pPageRight]/20);
            strcat(preambleOurDefs,buf);
            snprintf(buf, 100, "\\setlength{\\textwidth}{%dpt}\n", (prefs[pPageWidth] - prefs[pPageLeft] - prefs[pPageRight])/20);
            strcat(preambleOurDefs,buf);
        }
        
        if (!prefs[pConvertTextNoTab])
            strcat(preambleOurDefs,"\\newcommand{\\tab}{\\hspace{5mm}}\n\n");
        PutLitByteStr(preambleOurDefs);

        beginDocumentPos = ftell(ofp);
        PutLitStr("\\begin{document}\n");
        wroteBeginDocument = true;
        return 1;
    }
    return 0;
}

/*
 * This function initializes the text style.
 */
static void InitTextStyle(void)
{
    textStyle.fontSize = normalSize;

    textStyle.bold = 0;
    textStyle.italic = 0;
    textStyle.underlined = 0;
    textStyle.dbUnderlined = 0;
    textStyle.smallCaps = 0;
    textStyle.subScript = 0;
    textStyle.superScript = 0;
    textStyle.foreColor = 0;
    textStyle.fontNumber = -1;
    textStyle.charCode = genCharCode;

    textStyle.allCaps = 0;
    textStyle.backColor = 0;
}

/*
 * This function initializes the paragraph style.
 */
static void InitParagraphStyle(void)
{
    paragraph.firstIndent = 0;
    paragraph.leftIndent = 0;
    paragraph.rightIndent = 0;
    paragraph.spaceBefore = 0;
    paragraph.extraIndent = 0;
    paragraph.styleIndex = -1;
}

static int SameTextStyle(void)
{
    if (prefs[pConvertTextSize] && textStyleWritten.fontSize != textStyle.fontSize) return false;

    if (prefs[pConvertTextColor] && textStyleWritten.foreColor != textStyle.foreColor) return false;

    if (textStyleWritten.superScript != textStyle.superScript) return false;

    if (textStyleWritten.subScript != textStyle.subScript) return false;

    if (!prefs[pConvertTextForm]) return true;
    
    if (textStyleWritten.italic != textStyle.italic) return false;
    
    if (textStyleWritten.bold != textStyle.bold)  return false;

    if (textStyleWritten.underlined != textStyle.underlined) return false;

    if (textStyleWritten.smallCaps != textStyle.smallCaps) return false;

    if (textStyleWritten.dbUnderlined != textStyle.dbUnderlined) return false;

    if (textStyleWritten.charCode != textStyle.charCode) return false;

    return true;
}

/*
 * This function ends all text styles
 */
static void StopTextStyle(void)
{
    if (prefs[pConvertTextSize] && textStyleWritten.fontSize != normalSize) {
        PutLitStr("}");
        textStyleWritten.fontSize=normalSize;
    }

    if (prefs[pConvertTextColor] && textStyleWritten.foreColor) {
        PutLitStr("}");
        textStyleWritten.foreColor=0;
    }

    if (textStyleWritten.subScript) {
        PutLitStr("}");
        textStyleWritten.subScript=false;
    }

    if (textStyleWritten.superScript) {
        PutLitStr("}");
        textStyleWritten.superScript=false;
    }

    if (!prefs[pConvertTextForm]) return;

    if (textStyleWritten.italic) {
        PutLitStr("}");
        textStyleWritten.italic=false;
    }

    if (textStyleWritten.bold) {
        PutLitStr("}");
        textStyleWritten.bold=false;
    }

    if (textStyleWritten.smallCaps) {
        PutLitStr("}");
        textStyleWritten.smallCaps=false;
    }

    if (textStyleWritten.underlined) {
        PutLitStr("}");
        textStyleWritten.underlined=false;
    }

    if (textStyleWritten.dbUnderlined) {
        PutLitStr("}");
        textStyleWritten.dbUnderlined=false;
    }
}

/*
 * Alters the written latex style so it matches the current RTF style
 * This should only be called right before emitting a character
 */
static void WriteTextStyle(void)
{
    char buf[100];

    if (SameTextStyle()) return;
    
    if (wroteBeginDocument || !insideHeaderFooter) StopTextStyle();
    
    if (prefs[pConvertTextSize] && textStyleWritten.fontSize != textStyle.fontSize) {
        if (textStyle.fontSize != normalSize) {
            snprintf(buf, 100, "{%s ", fontSizeList[textStyle.fontSize]);
            PutLitStr(buf);
        }
        textStyleWritten.fontSize=textStyle.fontSize;
    }

    if (textStyleWritten.superScript != textStyle.superScript) {
        if (textStyle.superScript)
            PutLitStr("\\textsuperscript{");
        textStyleWritten.superScript=textStyle.superScript;
    }

    if (textStyleWritten.subScript != textStyle.subScript) {
        if (textStyle.subScript)
            PutLitStr("\\textsubscript{");
        textStyleWritten.subScript=textStyle.subScript;
        requireFixLtx2ePackage = true;
    }

    if (prefs[pConvertTextColor] && textStyleWritten.foreColor != textStyle.foreColor) {
        if (textStyle.foreColor) {
            snprintf(buf, 100, "{\\color{color%02d} ", textStyle.foreColor);
            PutLitStr(buf);
            UsedColor[textStyle.foreColor] = 1;
        }
        textStyleWritten.foreColor=textStyle.foreColor;
    }

    if (!prefs[pConvertTextForm]) return;

    if (textStyleWritten.italic != textStyle.italic) {
        if (textStyle.italic)
            PutLitStr("\\textit{");
        textStyleWritten.italic=textStyle.italic;
    }

    if (textStyleWritten.bold != textStyle.bold) {
        if (textStyle.bold)
            PutLitStr("\\textbf{");
        textStyleWritten.bold=textStyle.bold;
    }

    if (textStyleWritten.underlined != textStyle.underlined) {
        if (textStyle.underlined)
            PutLitStr("\\emph{");
        requireUlemPackage = true;
        textStyleWritten.underlined=textStyle.underlined;
    }

    if (textStyleWritten.smallCaps != textStyle.smallCaps) {
        if (textStyle.smallCaps)
            PutLitStr("\\textsc{");
        textStyleWritten.smallCaps=textStyle.smallCaps;
    }

    if (textStyleWritten.dbUnderlined != textStyle.dbUnderlined) {
        if (textStyle.dbUnderlined)
            PutLitStr("\\uuline{");
        requireUlemPackage = true;
        textStyleWritten.dbUnderlined=textStyle.dbUnderlined;
    }

    if (textStyleWritten.charCode != textStyle.charCode) {
    	curCharCode = textStyle.charCode;
        textStyleWritten.charCode=textStyle.charCode;
    }
}

/*
 * This handles font changing
 * 
 * When switching fonts like Times -> Helvetica
 *  
 *   (1) the style might change from roman to sans serif
 *   (2) the character code translation may change because
 *       the font is encoded with a different codepage
 */
static void SetFontStyle(void)
{
	RTFFont *font;

	textStyle.fontNumber = rtfParam;
	font = RTFGetFont(rtfParam);
//	fprintf(stderr, "Font %3d, cs=%3d, lookup=%p, name='%s'\n", font->rtfFNum, font->rtfFCharSet, font->rtfFCharCode, font->rtfFName);
	textStyle.charCode = font->rtfFCharCode;
	curCharCode = font->rtfFCharCode;
}

/*
 * This function stores the text style.
 */
static void SetTextStyle(void)
{
	if (insideHyperlink)
        return;

    switch (rtfMinor) {
    case rtfPlain:
        InitTextStyle();
        break;
    case rtfSmallCaps:
        textStyle.smallCaps = (rtfParam) ? true : false;
        break;
    case rtfAllCaps:
        textStyle.allCaps = (rtfParam) ? true : false;
        break;
    case rtfItalic:
        textStyle.italic = (rtfParam) ? true : false;
        break;
    case rtfBold:
        textStyle.bold = (rtfParam) ? true : false;
        break;
    case rtfUnderline:
        textStyle.underlined = (rtfParam) ? true : false;
        break;
    case rtfNoUnderline:
        textStyle.underlined = false;
        break;    
    case rtfDbUnderline:
        textStyle.dbUnderlined = (rtfParam) ? true : false;
        break;
    case rtfForeColor:
        textStyle.foreColor = rtfParam;
        break;
    case rtfSubScrShrink:
    case rtfSubScript:
        textStyle.subScript = (rtfParam) ? true : false;
        textStyle.superScript = false;
        break;
    case rtfSuperScrShrink:
    case rtfSuperScript:
        textStyle.superScript = (rtfParam) ? true : false;
        textStyle.subScript = false;
        break;
    case rtfFontSize:
        textStyle.fontSize = normalSize;
        if (rtfParam <= 12)
            textStyle.fontSize = scriptSize;
        else if (rtfParam <= 14)
            textStyle.fontSize = footNoteSize;
        else if (rtfParam <= 18)
            textStyle.fontSize = smallSize;
        else if (rtfParam <= 24)
            textStyle.fontSize = normalSize;
        else if (rtfParam <= 28)
            textStyle.fontSize = largeSize;
        else if (rtfParam <= 32)
            textStyle.fontSize = LargeSize;
        else if (rtfParam <= 36)
            textStyle.fontSize = LARGESize;
        else if (rtfParam <= 48)
            textStyle.fontSize = giganticSize;
        else if (rtfParam <= 72)
            textStyle.fontSize = GiganticSize;
        break;
    case rtfFontNum:
    	SetFontStyle();
    	break;
    case rtfDeleted:
        RTFSkipGroup();
        break;
    }
}

static void setLineSpacing(void)
{
    char buff[100];
    
    if (!prefs[pConvertLineSpacing]) return;
    
    if (paragraphWritten.lineSpacing == paragraph.lineSpacing)
        return;

    snprintf(buff, 100, "\\baselineskip=%dpt\n", abs(paragraph.lineSpacing)/20);
    PutLitStr(buff);

    if (g_debug_par_start) {
        snprintf(buff, 100, "[ls=%d]", paragraph.lineSpacing);
        PutLitStr(buff);
    }

    paragraphWritten.lineSpacing = paragraph.lineSpacing;
}

static void NewParagraph(void)
{
    char buff[100];

    (void) CheckForBeginDocument();

    nowBetweenParagraphs = false;

    if (insideFootnote || insideTable || insideHeaderFooter) return;

    if (prefs[pConvertInterParagraphSpace]) {
        if (current_vspace || paragraph.spaceBefore) {
            snprintf(buff,100,"\\vspace{%dpt}\n", (current_vspace+paragraph.spaceBefore)/20);
            PutLitStr(buff);
            current_vspace = 0;
        }
    }

    if (prefs[pConvertParagraphStyle] && paragraph.styleIndex != -1) {
        PutLitStr(Style2LatexOpen[paragraph.styleIndex]);
        paragraphWritten.styleIndex = paragraph.styleIndex;
        suppressLineBreak = true;
        return;
    }


    if (prefs[pConvertParagraphAlignment]) {
        if (paragraphWritten.alignment != paragraph.alignment) {
    
            if (paragraph.alignment == right)
                PutLitStr("\\begin{flushright}\n");
    
            if (paragraph.alignment == center)
                PutLitStr("\\begin{center}\n");
    
            paragraphWritten.alignment = paragraph.alignment;
        }
    }

    setLineSpacing();

    if (prefs[pConvertParagraphMargin]) {
        if (paragraphWritten.leftIndent != paragraph.leftIndent) {
            snprintf(buff, 100, "\\leftskip=%dpt\n", paragraph.leftIndent/20);
            if (paragraph.alignment != right && paragraph.alignment != center) {
                PutLitStr(buff);
                paragraphWritten.leftIndent = paragraph.leftIndent;
            }
        }
    }

    if (prefs[pConvertParagraphIndent]) {
        if (paragraphWritten.firstIndent != paragraph.firstIndent+paragraph.extraIndent) {
            snprintf(buff, 100, "\\parindent=%dpt\n", (paragraph.firstIndent+paragraph.extraIndent)/20);
            if (paragraph.alignment != right && paragraph.alignment != center) {
                PutLitStr(buff);
                paragraphWritten.firstIndent = paragraph.firstIndent+paragraph.extraIndent;
            }
            paragraph.extraIndent=0;
        }
    }

    if (section.cols > 1) {
        snprintf(buff, 100, "\n\\begin{multicols}{%d}\n", section.cols);
        PutLitStr(buff);
        requireMultiColPackage = true;
    }
}

/* 
 * Everything that is emitted is in some sort of paragraph
 * This routine just closes the environments that have been written
 */

static void EndParagraph(void)
{
    char buf[rtfBufSiz];

	if (insideHeaderFooter) return;
	
    if (CheckForBeginDocument()) return;

    StopTextStyle();

    if (insideTable) {
        PutLitStr("\\linebreak\n");
        return;
    }
	
    if (insideFootnote) {
        InsertNewLine();
        InsertNewLine();
        return;
    }

    if (prefs[pConvertParagraphStyle] && (paragraphWritten.styleIndex != -1)) {
        PutLitStr(Style2LatexClose[paragraphWritten.styleIndex]);
        suppressLineBreak = false;
        paragraphWritten.styleIndex=-1;
        InsertNewLine();
        InsertNewLine();
        return;
    }

    if (paragraphWritten.alignment != paragraph.alignment) {

        if (g_debug_par_start) {
            snprintf(buf, rtfBufSiz, "[oldalign=%d, newalign=%d]", paragraphWritten.alignment, paragraph.alignment);
            PutLitStr(buf);
        }

        if (paragraphWritten.alignment == right)
            PutLitStr("\n\\end{flushright}");

        if (paragraphWritten.alignment == center)
            PutLitStr("\n\\end{center}");
    }

    InsertNewLine();
    InsertNewLine();
}


static void setPreamblePackages(int ignoreUsedColor)
{
    if (!preamblePackages) 
        preamblePackages = malloc(1024);
    
    preamblePackages[0] = '\0';
    if (requireSetspacePackage)
        strcat(preamblePackages,"\\usepackage{setspace}\n");
    if (requireGraphicxPackage)
        strcat(preamblePackages,"\\usepackage{graphicx}\n");
    if (requireTablePackage)
        strcat(preamblePackages,"\\usepackage{longtable}\n");
    if (requireMultirowPackage)
        strcat(preamblePackages,"\\usepackage{multirow}\n");
    if (requireAmsSymbPackage)
        strcat(preamblePackages,"\\usepackage{amssymb}\n");
    if (requireMultiColPackage)
        strcat(preamblePackages,"\\usepackage{multicol}\n");
    if (requireUlemPackage)
        strcat(preamblePackages,"\\usepackage{ulem}\n");
    if (requireFixLtx2ePackage)
        strcat(preamblePackages,"\\usepackage{fixltx2e}\n");
    if (requireAmsMathPackage)
        strcat(preamblePackages,"\\usepackage{amsmath}\n");
    if (requireHyperrefPackage) {
        strcat(preamblePackages,"\\usepackage{hyperref}\n");
    }

    /* almost certainly want these packages for russian */
    if (genCharCode == cp1251CharCode) {
        strcat(preamblePackages,"\\usepackage[T2A]{fontenc}\n");
        strcat(preamblePackages,"\\usepackage[russian]{babel}\n");
    }
    
    if (requireFancyHdrPackage) {
        strcat(preamblePackages,"\\usepackage{fancyhdr}\n");
        strcat(preamblePackages,"\\renewcommand{\\headrulewidth}{0pt}\n");
        strcat(preamblePackages,"\\renewcommand{\\footrulewidth}{0pt}\n");
    }
    
    if (prefs[pConvertTextColor]) {
        int i=0;
        int needPackage=false;
        
        for (i=0; i<256; i++) 
            if (UsedColor[i]) needPackage = true;
        
        if (ignoreUsedColor || (!ignoreUsedColor && needPackage))
            strcat(preamblePackages,"\\usepackage{color}\n");
    }
}

/*
 * This function writes the LaTeX header and includes some basic packages.
 */
static void WriteLaTeXHeader(void)
{
    int i, j;

    PutLitStr(preambleFirstText);  /* from pref/r2l-pref     */
    InsertNewLine();
    PutLitStr(preambleSecondText); /* from pref/r2l-pref     */
    InsertNewLine();
    PutLitStr(preambleDocClass);   /* from pref/r2l-pref     */
    InsertNewLine();

    /* insert latex-encoding qualifier */
    PutLitStr(preambleUserText);

    /* insert latex-encoding qualifier */
    PutLitStr(preambleEncoding);

    /* to come back and write necessary \usepackage{...}
     * commands if necessary */
    packagePos = ftell(ofp);

    for (j = 0; j < PACKAGES + 1; j++) {
        for (i = 0; i < 100; i++)
            PutLitChar(' ');
        PutLitChar('\n');
    }
}

static void DoSectionCleanUp(void)
{
    if (section.cols > 1) {
        if (nowBetweenParagraphs)  /*finish paragraph before multicols */
            EndParagraph();
        PutLitStr("\n\\end{multicols}");
        InsertNewLine();
        section.cols = 1;
    }
}

/* This function causes the present group to be skipped.  */
static void SkipGroup(void)
{
    RTFSkipGroup();
    RTFRouteToken();
}

/* This function make sure that the output TeX file does not
 * contain one very, very long line of text.
 */
static void WrapText(void)
{
    if (wrapCount < WRAP_LIMIT) return;

    if (rtfMinor == rtfSC_space)
        PutLitChar('\n');
}

static void PrepareForChar(void)
{
    if (nowBetweenParagraphs) {

        if (rtfMinor == rtfSC_space) {
            paragraph.extraIndent += 72;
            return;
        }

        EndParagraph();
        NewParagraph();
    }

    if (insideHyperlink) {
        switch (rtfMinor) {
        case rtfSC_underscore:
            PutLitChar('H');
            return;
        case rtfSC_backslash:
            RTFGetToken();  /* ignore backslash */
            RTFGetToken();  /* and next character */
            return;
        }
    }

    if (rtfMinor >= rtfSC_therefore && rtfMinor < rtfSC_currency)
        requireAmsSymbPackage = true;

    WriteTextStyle();
}

/*
 * Write out a character.  rtfMajor contains the input character, rtfMinor
 * contains the corresponding standard character code.
 *
 * If the input character isn't in the charset map, try to print some
 * representation of it.
 */

static void TextClass(void)
{
	PrepareForChar();
    PutStdChar(rtfMinor);
    WrapText();
}

/* reads footnote. Just puts a footnote wrapper around whatever is
 * inside the footnote. Table footnotes are skipped for now
 * until I figure out a way that TeX likes.
 */
static void ReadFootnote(void)
{
    int footnoteGL;

    StopTextStyle();
    footnoteGL = braceLevel;
    PutLitStr("\\footnote{");
    insideFootnote = true;
    nowBetweenParagraphs = false;  /*no need to end last paragraph */
    while (braceLevel >= footnoteGL) {
        RTFGetToken();
        RTFRouteToken();
    }
    PutLitStr("}");
    insideFootnote = false;
}

/*
 * This routine sets attributes for the detected cell and
 * adds it to the table.cellInfo list. Memory for cells is
 * allocated dynamically as each cell is encountered.
 */
static void ReadCell(void)
{
    cell *cellPtr;
    char *fn = "ReadCell";

    cellPtr = New(cell);
    if (!cellPtr) {
        RTFPanic("%s: cannot allocate cell entry", fn);
        exit(1);
    }

    /*fprintf(stderr,"creating cell %d, (x,y)=(%d,%d), right=%d\n",
              table.cellCount, table.rows, (table.rowInfo)[table.rows], rtfParam);*/
    cellPtr->nextCell = table.cellInfo;
    cellPtr->x = table.rows;
    cellPtr->y = (table.rowInfo)[table.rows];
    if (table.cols == 0)
        cellPtr->left = table.leftEdge;
    else
        cellPtr->left = (table.cellInfo)->right;
    cellPtr->right = rtfParam;
    cellPtr->width = (double) (cellPtr->right - cellPtr->left) / rtfTpi;
    cellPtr->index = table.cellCount;
    cellPtr->mergePar = table.cellMergePar;
    table.cellMergePar = none;  /* reset */
    table.cellInfo = cellPtr;

    if (cellPtr->right <= table.previousColumnValue)
        cellPtr->right = table.previousColumnValue + 100;

    table.previousColumnValue = cellPtr->right;

}

static void DoTableAttr(void)
{
    switch (rtfMinor) {
    case rtfRowLeftEdge:
        table.leftEdge = rtfParam;
        break;
    case rtfCellPos:
        ReadCell();
        table.cellCount++;
        ((table.rowInfo)[table.rows])++;
        (table.cols)++;
        break;
    case rtfVertMergeRngFirst:
        table.cellMergePar = first;
        break;
    case rtfVertMergeRngPrevious:
        table.cellMergePar = previous;
        break;
    }
}


/*
 * This function searches the cell list by cell index
 * returns NULL if not found
 */
static cell *GetCellInfo(int cellNum)
{
    cell *cellPtr;

    if (cellNum == -1)
        return (table.cellInfo);

    for (cellPtr = (table.cellInfo); cellPtr != NULL; cellPtr = cellPtr->nextCell) {
        if (cellPtr->index == cellNum)
            return cellPtr;
    }

    if (!cellPtr)
        RTFPanic("GetCellInfo: Attempting to access invalid cell at index %d\n", cellNum);

    return NULL;
}


/*
 * This function searches the cell list by cell coordinates
 * returns NULL if not found
 */
static cell *GetCellByPos(int x, int y)
{
    cell *cellPtr;

    if (!table.cellInfo)
        RTFPanic("GetCellByPos: Attempting to access invalid cell at %d, %d\n", x, y);

    for (cellPtr = table.cellInfo; cellPtr != NULL; cellPtr = cellPtr->nextCell) {
        if (cellPtr->x == x && cellPtr->y == y)
            return cellPtr;
    }

    return NULL;
}



/*
 * In RTF, each table row need not start with a table row definition.
 * The next row may decide to use the row definition of the previous
 * row. In that case, I need to call this InheritTableRowDef function
 */
static void InheritTableRowDef(void)
{
    int prevRow;
    int cellsInPrevRow;
    cell *cellPtr, *newCellPtr;
    int i;
    char *fn = "InheritTableRowDef";

    prevRow = table.rows;
    cellsInPrevRow = (table.rowInfo)[prevRow];

    (table.rowInfo)[prevRow + 1] = (table.rowInfo)[prevRow];

    for (i = 0; i < cellsInPrevRow; i++) {

        cellPtr = GetCellByPos(prevRow, i);

        newCellPtr = New(cell);
        if (!newCellPtr) {
            RTFPanic("%s: cannot allocate inheriting cell entry", fn);
            exit(1);
        }

        newCellPtr->nextCell = table.cellInfo;
        newCellPtr->x = prevRow + 1;
        newCellPtr->y = cellPtr->y;
        newCellPtr->left = cellPtr->left;
        newCellPtr->right = cellPtr->right;
        newCellPtr->width = cellPtr->width;
        newCellPtr->index = table.cellCount;
        newCellPtr->mergePar = cellPtr->mergePar;
        table.cellMergePar = none;      /* reset */
        table.cellInfo = newCellPtr;
        table.cellCount++;
    }
}

/*
 * This function counts the number of columns a cell spans.
 * This is done by comparing the cell's left and right edges
 * to the sorted column border array. If the left and right
 * edges of the cell are not consecutive entries in the array,
 * the cell spans multiple columns.
 */
static int GetColumnSpan(cell * cellPtr)
{
    int i, j;

    /* if this is the last cell in the row, make its right edge flush with the table right edge */
    if (cellPtr->y == ((table.rowInfo)[cellPtr->x]) - 1)
        cellPtr->right = (table.columnBorders)[table.cols];

    for (i = 0; i < table.cols; i++) {
        if ((table.columnBorders)[i] == cellPtr->left)
            break;
    }

    for (j = i; j < table.cols + 1; j++){
        if ((table.columnBorders)[j] == cellPtr->right)
            break;
    }

    return (j - i);
}


/*
 * This routine prescans the table.

 * This is tricky because RTF does not really have a table construct.  Instead, each
 * row is laid out as a series of cells with markup for each.
 * It counts how many rows there are in the table
 * and the number of cells in each row. It also calculates the cell widths. Finally, it
 * builds an array of column borders that is useful in figuring out whether a cell spans
 * multiple columns. It has lots of diagnostic printf statements. Shows how much I
 * struggled, and how much I trust it.
 *
*/
static void PrescanTable(void)
{
    size_t tableStart;
    boolean foundRow = true;
    boolean foundColumn = true;
    int i, j;
    cell *cellPtr, *cellPtr1;
    char *fn = "PrescanTable";
    short prevChar;
    int maxCols = 0;
    int tableLeft, tableRight, tableWidth;
    int *rightBorders;
    boolean enteredValue;

    RTFStoreStack();
    prevChar = RTFPushedChar();

    /* mark the current cursor position */
    tableStart = ftell(ifp);

    RTFGetToken();

    table.rows = 0;
    table.cols = 0;
    insideTable = true;
    table.cellInfo = NULL;

    /* Prescan each row until end of the table. */
    while (foundRow) {
        table.cols = 0;
        (table.rowInfo)[table.rows] = 0;

        while (foundColumn) {
            RTFGetToken();
            if (RTFCheckMM(rtfSpecialChar, rtfOptDest))
                RTFSkipGroup();

            else if (RTFCheckCM(rtfControl, rtfTblAttr))
                RTFRouteToken();

            else if (RTFCheckMM(rtfSpecialChar, rtfRow) || rtfClass == rtfEOF)
                foundColumn = false;
        }

        if (g_debug_table_prescan) fprintf(stderr,"* reached end of row %d\n", table.rows);

        insideTable = false;
        table.newRowDef = false;

        /* table row should end with rtfRowDef, rtfParAttr */
        while (1) {

            RTFGetToken();

            if (RTFCheckMM(rtfSpecialChar, rtfOptDest)) {
                RTFSkipGroup();
                continue;
            }

            if (RTFCheckMM(rtfTblAttr, rtfRowDef)) {
                insideTable = true;
                table.newRowDef = true;
                break;
            }

            if (rtfClass == rtfText)
                break;

            if (RTFCheckCM(rtfControl, rtfSpecialChar))
                break;

            if (RTFCheckMM(rtfParAttr, rtfInTable)) {
                insideTable = true;
                break;
            }

            if (rtfClass == rtfEOF)
                break;
        }

        if (!insideTable)
            foundRow = false;
        else
            foundRow = true;


        if ((table.rowInfo)[table.rows] == 0)
            (table.rowInfo)[table.rows] = (table.rowInfo)[table.rows - 1];

        if (table.cols > maxCols)
            maxCols = table.cols;


        if (g_debug_table_prescan) fprintf(stderr,"* read row %d with %d cells\n", table.rows, (table.rowInfo)[table.rows]);
        (table.rows)++;
        table.previousColumnValue = PREVIOUS_COLUMN_VALUE;

        if (g_debug_table_prescan) fprintf(stderr,"* inside table is %d, new row def is %d\n", insideTable, table.newRowDef);

        if (insideTable && !(table.newRowDef)) {
            (table.rows)--;
            InheritTableRowDef();
            if (g_debug_table_prescan) fprintf(stderr,"* Inherited: read row %d with %d cells\n", table.rows, (table.rowInfo)[table.rows]);
            (table.rows)++;
        }

        foundColumn = true;
    }


    table.cols = maxCols;

    /*************************************************************************
     * Determine the number of columns and positions in the table by creating
     * a list containing one entry for each unique right border
     * This list is retained as table.columnBorders
     * The number of columns is table.cols
    */

    /* largest possible list */
    rightBorders = (int *) RTFAlloc((table.cellCount) * sizeof(int));
    if (!rightBorders) {
        RTFPanic("%s: cannot allocate array for cell borders\n", fn);
        exit(1);
    }

    table.cols = 0;
    for (cellPtr = table.cellInfo; cellPtr != NULL; cellPtr = cellPtr->nextCell) {

        enteredValue = false;
        for (j = 0; j < table.cols; j++)
            if (rightBorders[j] == cellPtr->right) enteredValue=true;

        if (!enteredValue) {
            rightBorders[table.cols] = cellPtr->right;
            (table.cols)++;
        }

        if (cellPtr->y == 0)
            cellPtr->left = table.leftEdge;
    }

    /* allocate array for column border entries. */
    table.columnBorders = (int *) RTFAlloc(((table.cols) + 1) * sizeof(int));

    if (!table.columnBorders) {
        RTFPanic("%s: cannot allocate array for column borders\n", fn);
        exit(1);
    }

    for (i = 0; i < table.cols; i++)
        (table.columnBorders)[i + 1] = rightBorders[i];

    RTFFree((char *)rightBorders);


    /******* Table parsing can be messy, and it is still buggy. *******/

    if (g_debug_table_prescan) fprintf(stderr,"* table has %d rows and %d cols \n", table.rows, table.cols);

    (table.columnBorders)[0] = table.leftEdge;

    /***************************************************************************
     * sort the column border array in ascending order.
     * This is important for figuring out whether a cell spans multiple columns.
     * This is calculated in the function GetColumnSpan.
     */

    if (g_debug_table_prescan) fprintf(stderr,"* sorting borders...\n");
    for (i = 0; i < (table.cols); i++)
        for (j = i + 1; j < (table.cols + 1); j++)
            if ((table.columnBorders)[i] > (table.columnBorders)[j])
                Swap((table.columnBorders)[i], (table.columnBorders)[j]);

    if (g_debug_table_prescan) fprintf(stderr,"* table left border is at %d\n", (table.columnBorders)[0]);

    for (i = 1; i < (table.cols + 1); i++) {
        /* Sometimes RTF does something stupid and assigns two adjoining cell
           the same right border value. Dunno why. If this happens, I move the
           second cell's right border by 10 twips. Microsoft really has some
           morons!
         */
        if (table.columnBorders[i] == table.columnBorders[i - 1])
            table.columnBorders[i] += 10;

        if (g_debug_table_prescan) fprintf(stderr,"* cell right border is at %d\n", table.columnBorders[i]);
    }

    /* fill in column spans for each cell */
    for (i = 0; i < table.cellCount; i++) {
        cellPtr = GetCellInfo(i);
        if (!cellPtr){
            RTFPanic("%s: Attempting to access invalid cell at index %d\n", fn, i);
            exit(1);
        }

        cellPtr->columnSpan = GetColumnSpan(cellPtr);
        if (cellPtr->columnSpan > 1)
            table.multiCol = true;

        if (g_debug_table_prescan) fprintf(stderr,"* cell %d spans %d columns\n", cellPtr->index, cellPtr->columnSpan);

        /* correct the vertical cell position for any multicolumn cells */
        if ((cellPtr->y) != 0) {
            cellPtr1 = GetCellInfo(i - 1);
            if (cellPtr == NULL)
                RTFPanic ("%s: Attempting to access invalid cell at index %d\n", fn, i);
            cellPtr->y = cellPtr1->y + cellPtr1->columnSpan;
        }

        tableLeft = (table.columnBorders)[0];
        tableRight = (table.columnBorders)[table.cols];
        tableWidth = tableRight - tableLeft;

        cellPtr->width = (double) (cellPtr->right - cellPtr->left) / rtfTpi;
        if (tableWidth > 4.5 * rtfTpi)
            cellPtr->width *= 4.5 * rtfTpi / tableWidth;

    }

    if (g_debug_table_prescan) {
        for (cellPtr = table.cellInfo; cellPtr != NULL; cellPtr = cellPtr->nextCell) {
            fprintf(stderr,"* cell #%d (%d, %d) ", cellPtr->index, cellPtr->x, cellPtr->y);
            fprintf(stderr,"left=%5d right=%5d ", cellPtr->left, cellPtr->right);
            fprintf(stderr,"and spans %d columns\n", cellPtr->columnSpan);
        }
    }

    /* go back to beginning of the table */
    fseek(ifp, tableStart, 0);
    RTFSimpleInit();
    RTFSetPushedChar(prevChar);
    RTFRestoreStack();
}

/*
 * This routine counts the number of cells to be merged vertically and writes the
 * corresponding \multirow statement.
 */
static void DoMergedCells(cell * cellPtr)
{
    int i;
    int x, y;
    cell *localCellPtr;
    char buf[rtfBufSiz];

    x = cellPtr->x;
    y = cellPtr->y;

    i = 1;
    localCellPtr = GetCellByPos(x + i, y);
    for (i = 1; localCellPtr->mergePar == previous; i++)
        if (x + i > table.rows - 1)
            break;
        else
            localCellPtr = GetCellByPos(x + i, y);

    snprintf(buf, rtfBufSiz, "\\multirow{%d}{%1.3fin}{%s ", i - 1, cellPtr->width, justificationList[paragraph.alignment]);
    PutLitStr(buf);

    table.multiRow = true;
    requireMultirowPackage = true;
}


/*
 * Writes cell information for each cell. Each cell is defined in a multicolumn
 * environment for maximum flexibility. Useful when we have merged rows and columns.
 */
static void WriteCellHeader(int cellNum)
{
    char buf[rtfBufSiz];
    cell *cellPtr;

    if (g_debug_table_writing) fprintf(stderr,"* Writing cell header for cell #%d\n",cellNum);

    cellPtr = GetCellInfo(cellNum);
    if (!cellPtr) {
        RTFPanic("WriteCellHeader: Attempting to access invalid cell at index %d\n", cellNum);
        exit(1);
    }

    if (table.multiCol) {
        snprintf(buf, rtfBufSiz, "\\multicolumn{%d}{", cellPtr->columnSpan);
        PutLitStr(buf);

        if (cellPtr->columnSpan < 1)
            RTFMsg("* Warning: nonsensical table encountered...cell %d spans %d columns.\nProceed with caution!\n",
                   cellPtr->index, cellPtr->columnSpan);

        /* this check is to draw the left vertical boundary of the table */
        if (cellPtr->y == 0)
            PutLitChar('|');

        if (cellPtr->mergePar == first) {
            snprintf(buf, rtfBufSiz, "p{%1.3fin}|}{\\begin{minipage}[t]{%1.3fin}", cellPtr->width, cellPtr->width);
            PutLitStr(buf);
        } else {
            snprintf(buf, rtfBufSiz, "p{%1.3fin}|}{\\begin{minipage}[t]{%1.3fin}%s", cellPtr->width,cellPtr->width, justificationList[paragraph.alignment]);
            PutLitStr(buf);
            InsertNewLine();
        }
        
    } else if (cellPtr->mergePar != first) {
        snprintf(buf,rtfBufSiz, "\\begin{minipage}[t]{%1.3fin}%s ", cellPtr->width, justificationList[paragraph.alignment]);
        PutLitStr(buf);
    } else {
        snprintf(buf,rtfBufSiz, "\\begin{minipage}[t]{%1.3fin} ", cellPtr->width);
        PutLitStr(buf);
    }

    if (cellPtr->mergePar == first)
        DoMergedCells(cellPtr);

    if (g_debug_table_writing)  {
        snprintf(buf, rtfBufSiz, "[cell \\#%d]",cellNum);
        PutLitStr(buf);
    }
}

/* This is where we translate each table row to latex. */
static void ProcessTableRow(int rowNum)
{
    boolean cellIsEmpty = true;
    boolean firstCellInRow = true;
    cell *cellPtr;

    suppressLineBreak = true;
    while (1) {
        RTFGetToken();

        /* ignore these */
        if (RTFCheckCM(rtfControl, rtfTblAttr))
            continue;

        /* token that signals end of the row */
        if (RTFCheckCMM(rtfControl, rtfSpecialChar, rtfRow)) {
            if (g_debug_table_writing) fprintf(stderr,"* end of row\n");
            suppressLineBreak = false;
            return;
        }

        /* token that signals the end of the current cell */
        if (RTFCheckCMM(rtfControl, rtfSpecialChar, rtfCell)) {
            (table.cellCount)++;

            if (cellIsEmpty) {
                if (g_debug_table_writing) fprintf(stderr,"* cell #%d is empty\n", table.cellCount - 1);
                if (!firstCellInRow) PutLitStr(" & ");
                cellPtr = GetCellInfo(table.cellCount - 1);
                WriteCellHeader(table.cellCount - 1);
                firstCellInRow = false;
            } 
            
            StopTextStyle();
            if (cellPtr->mergePar == first)
                PutLitChar('}');

            PutLitStr("\\end{minipage}");
                
            if (table.multiCol)
                PutLitChar('}');

            cellIsEmpty = true;
            paragraph.alignment = left;
            continue;
        }

        if (cellIsEmpty) {
            if (rtfMajor == rtfDestination || rtfMajor ==rtfSpecialChar || rtfClass == rtfText) {
                if (!firstCellInRow)
                    PutLitStr(" & ");
                
                WriteCellHeader(table.cellCount);
                cellPtr = GetCellInfo(table.cellCount);
                cellIsEmpty = false;
                firstCellInRow = false;
            }
        }

        RTFRouteToken();
    }
}

/*
 * This function draws horizontal lines within a table. It looks
 * for vertically merged rows that do not have any bottom border
 */
static void DrawTableRowLine(int rowNum)
{
    int i, cellPosition;
    cell *cellInfo1, *cellInfo2;
    char buf[rtfBufSiz];

    /* if we are at the last row of the table, just draw a straight \hline. */
    if (rowNum == (table.rows) - 1 || !table.multiRow) {
        PutLitStr("\\hline");
        InsertNewLine();
        return;
    }

    /* otherwise use \cline for every cell */
    /* this is to count cell positions as if the table is a matrix. */
    cellPosition = 0;
    for (i = 0; i < (table.rowInfo)[rowNum]; i++) {

        cellInfo1 = GetCellByPos(rowNum, cellPosition);
        cellPosition += cellInfo1->columnSpan;

        if (cellInfo1->mergePar == none) {
            snprintf(buf, rtfBufSiz, "\\cline{%d-%d}", cellInfo1->y + 1, cellInfo1->y + cellInfo1->columnSpan);
            PutLitStr(buf);
            continue;
        }

        if (cellInfo1->mergePar == previous) {
            cellInfo2 = GetCellByPos(rowNum + 1, i);
            if (cellInfo2->mergePar != previous) {
                snprintf(buf, rtfBufSiz, "\\cline{%d-%d}", cellInfo1->y + 1, cellInfo1->y + cellInfo1->columnSpan);
                PutLitStr(buf);
            }
        }
    }

    InsertNewLine();
}


/* All right, the big monster. When we reach a table,
 * we don't know anything about it, i.e., number of rows
 * and columns, whether any rows or columns are merged,
 * etc. We have to prescan the table first, where we
 * get vital table parameters, and then come back to
 * read the table in actual.  This is necessary because
 * latex formats the table at the start.
 */
static void DoTable(void)
{
    int i, rowNum;
    cell *cellPtr;
    char buf[100];

    requireTablePackage = true;
    table.previousColumnValue = PREVIOUS_COLUMN_VALUE;

    /* throw away old cell information lists */
    while (table.cellInfo) {
        cellPtr = (table.cellInfo)->nextCell;
        RTFFree((char *) table.cellInfo);
        table.cellInfo = cellPtr;
    }

    /* Prescan table */
    table.cellCount = 0;
    PrescanTable();
    if (g_debug_table_writing) fprintf(stderr,"* done prescanning table...\n");
    table.cellCount = 0;

    insideTable = false;

    EndParagraph();
    NewParagraph();

    insideTable = true;

    PutLitStr("\\begin{");
    PutLitStr(convertTableName);
    PutLitStr("}{");
    if (table.multiCol) {
        for (i = 0; i < table.cols; i++)
            PutLitChar('l');
        PutLitStr("}");
    } else {
        PutLitStr("|");
        for (i = 0; i < table.cols; i++) {
            cellPtr = GetCellInfo(i);
            if (cellPtr->x > 0)
                break;
            snprintf(buf, 100, "p{%1.3fin}|", cellPtr->width);
            PutLitStr(buf);
        }
        PutLitStr("}");
    }
    PutLitStr("\n\\hline");
    InsertNewLine();


    paragraph.alignment = left; /* default justification */

    rowNum = 0;

/*      printf ("* processing table rows...\n");        */
    for (i = 0; i < table.rows; i++) {
        snprintf(buf, 100, "%% ROW %d\n", i + 1);
        PutLitStr(buf);

        if (g_debug_table_writing) fprintf(stderr,"* Starting new row #%d\n",i);
        ProcessTableRow(rowNum);

        PutLitStr("\\\\");
        InsertNewLine();
        if (i < (table.rows - 1))
            DrawTableRowLine(rowNum);
        paragraph.alignment = left;
        rowNum++;

        InitTextStyle();
    }

    PutLitStr("\\hline\n");
    snprintf(buf, 100, "\\end{%s}\n", convertTableName);
    PutLitStr(buf);
    nowBetweenParagraphs = true;

    RTFFree((char *) table.columnBorders);
    insideTable = false;       /* end of table */
    table.previousColumnValue = PREVIOUS_COLUMN_VALUE;
    table.multiCol = false;
    table.multiRow = false;

}

/* set paragraph attributes that might be useful */
static void ParAttr(void)
{
    if (insideFootnote || insideHyperlink || insideHeaderFooter)
        return;

    if (insideTable) {
        switch (rtfMinor) {
        case rtfQuadCenter:
            paragraph.alignment = center;
            break;
        case rtfQuadRight:
            paragraph.alignment = right;
            break;
        }
        return;
    }

    switch (rtfMinor) {
    case rtfSpaceBetween:
        paragraph.lineSpacing = rtfParam;
        break;
    case rtfQuadCenter:
        paragraph.alignment = center;
        break;
    case rtfQuadJust:
    case rtfQuadLeft:
        paragraph.alignment = left;
        break;
    case rtfQuadRight:
        paragraph.alignment = right;
        break;
        
    case rtfParDef:
        paragraph.firstIndent = 0;
        paragraph.leftIndent = 0;
        paragraph.extraIndent = 0;
        paragraph.alignment = left;
        paragraph.styleIndex = -1;
        break;
        
    case rtfStyleNum:
        if (prefs[pConvertParagraphStyle] && rtfParam < MAX_STYLE_MAPPINGS) 
            paragraph.styleIndex = Style2LatexMapIndex[rtfParam];
        else
            paragraph.styleIndex = -1;
        break;
        
    case rtfFirstIndent:
        paragraph.firstIndent = rtfParam;
        break;
    case rtfLeftIndent:
        paragraph.leftIndent = rtfParam;
        break;
    case rtfRightIndent:
        paragraph.rightIndent = rtfParam;
        break;
    case rtfSpaceBefore:
        paragraph.spaceBefore = rtfParam;
        break;
    case rtfSpaceAfter:
        paragraph.spaceAfter = rtfParam;
        break;
    }

}

static void SectAttr(void)
{
	if (insideHeaderFooter) return;
	
    switch (rtfMinor) {
    case rtfColumns:
        section.cols = rtfParam;
        section.newStyle = true;
        break;
    case rtfSectDef:
        DoSectionCleanUp();
        section.cols = 1;
/*                       section.newStyle = true; */
        break;
    }

}

/*
 * This function rewrites the LaTeX file with simpler header
 */
void EndLaTeXFile(void)
{
    FILE *nfp=NULL;
    int numr,len;
    char* newname;
    char* oldname;
    char buffer[512];
    
    /* last few bits */
    EndParagraph();
    DoSectionCleanUp();
    PutLitStr("\n\n\\end{document}\n");

    /* open new file, changing name from file.ltx to file.tex*/
    oldname = RTFGetOutputName();
    newname = strdup(oldname);
    len = strlen(newname);
    newname[len-3]='t';
    newname[len-2]='e';
    nfp = fopen(newname, "wb");
    if (!nfp) return;
    
    /* prepare */
    RTFSetOutputStream(nfp);
    RTFSetOutputName(newname);
    
    /* write improved header */
    suppressLineBreak = false;
    PutLitStr(preambleFirstText);  /* from pref/r2l-pref     */
    InsertNewLine();
    PutLitStr(preambleSecondText); /* from pref/r2l-pref     */
    InsertNewLine();
    PutLitStr(preambleDocClass);   /* from pref/r2l-pref     */
    InsertNewLine();
    PutLitStr(preambleEncoding);   /* from pref/latex-encoding */
    InsertNewLine();
    InsertNewLine();
    setPreamblePackages(false);
    PutLitStr(preamblePackages);   /* as needed */

    PutLitStr(preambleUserText);   /* from pref/r2l-head      */
    InsertNewLine();
    DefineColors(false);
    InsertNewLine();
    PutLitByteStr(preambleOurDefs);    /* e.g., \tab */
    InsertNewLine();

	if (preambleFancyHeader){
		PutLitByteStr("\\pagestyle{fancy}\n");
		PutLitByteStr("\\rhead{}\n\\rfoot{}\n\\chead{}\n\\cfoot{}\n");
		PutLitByteStr(preambleFancyHeader);
    	InsertNewLine();
	}
		
	if (preambleFancyHeaderFirst){
		PutLitByteStr("\\fancypagestyle{plain}{\n");
		PutLitByteStr("  \\rhead{}\n  \\rfoot{}\n  \\chead{}\n  \\cfoot{}\n");
		PutLitByteStr(preambleFancyHeaderFirst);
		PutLitByteStr("}\n");
		PutLitByteStr("\\thispagestyle{plain}\n");
    	InsertNewLine();
	}

    
    /* now copy the body of the document */
    fseek(ofp, beginDocumentPos, 0);

    while(!feof(ofp)){  
        numr = fread(buffer,1,512,ofp);
        fwrite(buffer,1,numr,nfp);
    }
    
    /* close files and delete old one */
    free(newname);
    fclose(ofp);
    fclose(nfp);
//    unlink(oldname);
}

/* sets the output stream */
void RTFSetOutputStream(stream)
FILE *stream;
{
    ostream = stream;
}

/* This function looks for the beginning of the hex data in a \pict structure */
static int HexData(void)
{
    /* get the next token */
    RTFGetToken();

    /* if we fall into a group, skip the whole she-bang */
    if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0) {
        RTFPeekToken();
        if ((int) (rtfTextBuf[0]) == 10) {
            RTFGetToken();      /* skip any carriage returns */
            RTFPeekToken();     /* look at the next token to
                                 * check if there is another row */
        }

        /*
         * there are some groups within the header that contain text data that should not
         * be confused with hex data
         */
        if (RTFCheckMM(rtfDestination, rtfSp) != 0 || strcmp(rtfTextBuf, "\\*") == 0)
        {
            RTFSkipGroup();
            return (0);
        }
    }

    /* paydirt, hex data starts */
    if (rtfClass == rtfText)
        return (1);

    /* no such luck, but set picture attributes when encountered */
    if (RTFCheckCM(rtfControl, rtfPictAttr) != 0) {
        switch (rtfMinor) {
        case rtfMacQD:
            picture.type = pict;
            break;
        case rtfWinMetafile:
            picture.type = wmf;
            break;
        case rtfEmf:
            picture.type = emf;
            break;
        case rtfPng:
            picture.type = png;
            break;
        case rtfJpeg:
            picture.type = jpeg;
            break;
        case rtfPicGoalWid:
            picture.goalWidth = rtfParam;
            break;
        case rtfPicGoalHt:
            picture.goalHeight = rtfParam;
            break;
        case rtfPicScaleX:
            picture.scaleX = rtfParam;
            break;
        case rtfPicWid:
            picture.width = rtfParam;
            break;
        case rtfPicHt:
            picture.height = rtfParam;
            break;
        case rtfPicScaleY:
            picture.scaleY = rtfParam;
            break;
        }
    }
    return (0);

}

/* some picture formats like PICT and WMF require headers that
 * the RTF file does not include.
 */
static void WritePictureHeader(FILE * pictureFile)
{
    unsigned char wmfhead[22] = {
        /* key      = */ 0xd7, 0xcd, 0xc6, 0x9a,
        /* hmf      = */ 0x00, 0x00,
        /* bbox     = */ 0xfc, 0xff, 0xfc, 0xff,
        /* width    = */ 0x00, 0x00,
        /* height   = */ 0x00, 0x00,
        /* inch     = */ 0x60, 0x00,
        /* reserved = */ 0x00, 0x00, 0x00, 0x00,
        /* checksum = */ 0x00, 0x00
    };
    int i;
    int height, width;

    if (picture.goalHeight == 0) {
        height = ((float) picture.height * picture.scaleY * 96) / ((float) rtfTpi * 100);
        width = (int)((float) picture.width * picture.scaleX * 96) / (rtfTpi * 100);
    } else {
        height = (int)((float) picture.goalHeight * picture.scaleY * 96) / (rtfTpi * 100);
        width = (int)((float) picture.goalWidth * picture.scaleX * 96) / (rtfTpi * 100);
    }
    wmfhead[10] = (width) % 256;
    wmfhead[11] = (width) / 256;
    wmfhead[12] = (height) % 256;
    wmfhead[13] = (height) / 256;

    /* compute Checksum */
    wmfhead[20] = 0;
    wmfhead[21] = 0;
    for (i = 0; i < 20; i += 2) {
        wmfhead[20] ^= wmfhead[i];
        wmfhead[21] ^= wmfhead[i + 1];
    }


    switch (picture.type) {
    case pict:                  /* write 512 byte empty header */
        for (i = 0; i < 512; i++)
            fputc(' ', pictureFile);
        break;
    case wmf:
        fwrite(wmfhead, 22, 1, pictureFile);
        break;
    case emf:
        break;
    case png:
        break;

    }

}

/* start reading hex encoded picture */
static void ConvertHexPicture(char *pictureType)
{
    FILE *pictureFile;
    char dummyBuf[rtfBufSiz];
    char pictByte;
    int groupEnd = false;
    short hexNumber;
    short hexEvenOdd = 0;       /* check if we read in even number of hex characters */

    strcpy(picture.name, "");

    /* increment the picture counter and clear dummy buffer */
    (picture.count)++;
    strcpy(dummyBuf, "");

    /* get input file name and create corresponding picture file name */
    if (pictureType == NULL)
        strcpy(pictureType, "unknown");

    strcpy(picture.name, RTFGetOutputName());
    picture.name[strlen(picture.name)-4] = '\0';
    snprintf(dummyBuf, rtfBufSiz, "-fig%03d.%s", picture.count, pictureType);
    strcat(picture.name, dummyBuf);

    /* open picture file */
    if ((pictureFile = fopen(picture.name, "wb")) == NULL)
        RTFPanic("Cannot open input file %s\n", picture.name);

    /* write appropriate header */
    WritePictureHeader(pictureFile);

    /* now we have to read the hex code in pairs of two
     * (1 byte total) such as ff, a1, 4c, etc...*/
    while (!groupEnd) {
        RTFGetToken();

        /* CR or LF in the hex stream should be skipped */
        if ((int) (rtfTextBuf[0]) == 10 || (int) (rtfTextBuf[0]) == 13)
            RTFGetToken();

        if (rtfClass == rtfGroup)
            break;

        if (!groupEnd) {
            hexNumber = 16 * RTFCharToHex(rtfTextBuf[0]);
            hexEvenOdd++;
        }

        RTFGetToken();
        if ((int) (rtfTextBuf[0]) == 10 || (int) (rtfTextBuf[0]) == 13)
            RTFGetToken();

        if (rtfClass == rtfGroup)
            break;

        if (!groupEnd) {
            hexNumber += RTFCharToHex(rtfTextBuf[0]);   /* this is the the number */
            hexEvenOdd--;
            /* shove that number into a character of 1 byte */
            pictByte = hexNumber;
            fputc(pictByte, pictureFile);
        }
    }

    if (fclose(pictureFile) != 0)
        printf("* error closing picture file %s\n", picture.name);
    if (hexEvenOdd)
        printf("* Warning! Odd number of hex characters read for picture %s\n",
             picture.name);
}


/* 
 * Write appropriate commands to include the picture
 */
static void IncludeGraphics(char *pictureType)
{
    char *figPtr, *suffix;
    char dummyBuf[rtfBufSiz];
    double scaleX, scaleY;
    int width, height;

#ifdef UNIX
    if (strcmp(pictureType, "pict") == 0) {
        if (!system("command -v pict2pdf") ) {
            int err;
            char *pdfname = strdup(picture.name);
            strcpy(pdfname + strlen(pdfname) - 4, "pdf");

            snprintf(dummyBuf, rtfBufSiz, "pict2pdf '%s' ", picture.name);            
            err = system(dummyBuf);

            if (!err) {
                unlink(picture.name);
                strcpy(picture.name,pdfname);
            }
            free(pdfname);
        }
    }

    if (strcmp(pictureType, "wmf") == 0) {
        if (!system("command -v wmf2eps") && !system("command -v epstopdf")) {
            int err;
            char *pdfname = strdup(picture.name);
            strcpy(pdfname + strlen(pdfname) - 3, "pdf");

            snprintf(dummyBuf, rtfBufSiz, "wmf2eps '%s' | epstopdf --filter > '%s'", picture.name, pdfname);            
            err = system(dummyBuf);

            if (!err) {
                unlink(picture.name);
                strcpy(picture.name,pdfname);
            }
            free(pdfname);
        }
    }
#endif
#ifdef MSWIN
    if (strcmp(pictureType, "wmf") == 0) {
        if (!system("which wmf2eps") && !system("which epstopdf")) {
            int err;
            char *pdfname = strdup(picture.name);
            strcpy(pdfname + strlen(pdfname) - 3, "pdf");

            snprintf(dummyBuf, rtfBufSiz, "w2p.bat %s %s", picture.name, pdfname);            
            err = system(dummyBuf);

            if (!err) {
                unlink(picture.name);
                strcpy(picture.name,pdfname);
            }
            free(pdfname);
        }
    }
#endif

    suffix = strrchr(picture.name, '.');

    if (picture.scaleX == 0)
        scaleX = 1;
    else
        scaleX = picture.scaleX / 100.0;
        
    if (picture.scaleY == 0)
        scaleY = 1;
    else
        scaleY = picture.scaleY / 100.0;

    if (picture.goalHeight == 0) {
        width = (int) picture.width * scaleX;
        height = (int) picture.height * scaleY;
    } else {
        width = (int) picture.goalWidth * scaleX / 20.0;
        height = (int) picture.goalHeight * scaleY / 20.0;
    }

    figPtr = strrchr(picture.name, PATH_SEP);
    if (!figPtr)
        figPtr = picture.name;
    else
        figPtr++;

    if (!insideTable && !insideFootnote) {
    	int oldWrittenAlignment = -1;
    
        EndParagraph();

        if (height > 50) 
            PutLitStr("\\begin{figure}[htbp]");
        
        if (height > 20) {
            PutLitStr("\n\\begin{center}");
            oldWrittenAlignment =  paragraphWritten.alignment;
            paragraphWritten.alignment = center;
        }
        
        snprintf(dummyBuf, rtfBufSiz, "\n\\includegraphics[width=%dpt, height=%dpt]{%s}", width, height, figPtr);
        PutLitStr(dummyBuf);
        
        if (height > 50) {
            PutLitStr("\n\\caption{This should be the caption for \\texttt{");
            PutEscapedLitStr(figPtr);
            PutLitStr("}.}");
        }
        
        if (height > 20) {
            PutLitStr("\n\\end{center}");
            paragraphWritten.alignment=oldWrittenAlignment;
        }

        if (height > 50) 
            PutLitStr("\n\\end{figure}");
            
        nowBetweenParagraphs = true;
    }
}

/* This function reads in a picture */
static void ReadPicture(void)
{
/*     char *fn = "ReadPicture"; */
    requireGraphicxPackage = true;
    picture.type = unknownPict;
    picture.width = 0;
    picture.height = 0;
    picture.goalWidth = 0;
    picture.goalHeight = 0;
    picture.scaleX = 100;
    picture.scaleY = 100;

/*     RTFMsg("%s: Starting ...\n",fn); */

    /* skip everything until we reach hex data */
    while (!HexData());

    /* Put back the first hex character into the stream (removed by HexData) */
    RTFUngetToken();

    /* Process picture */
    switch (picture.type) {
    case pict:
/*         RTFMsg("* Warning: PICT format image encountered.\n"); */
        ConvertHexPicture("pict");
        IncludeGraphics("pict");
        break;
    case wmf:
/*         RTFMsg("* Warning: WMF format image encountered.\n"); */
        ConvertHexPicture("wmf");
        IncludeGraphics("wmf");
        break;
    case emf:
/*         RTFMsg("* Warning: EMF format image encountered.\n"); */
        ConvertHexPicture("emf");
        IncludeGraphics("emf");
        break;
    case png:
/*         RTFMsg("* Warning: PNG format image encountered.\n"); */
        ConvertHexPicture("png");
        IncludeGraphics("png");
        break;
    case jpeg:
/*         RTFMsg("* Warning: JPEG format image encountered.\n"); */
        ConvertHexPicture("jpg");
        IncludeGraphics("jpg");
        break;
    default:
        ConvertHexPicture("unknown");
        printf("* Warning: unknown picture type encountered.\n");
        IncludeGraphics("unknown");
        break;
    }

    /* feed "}" back to router */
    RTFRouteToken();

    /* reset picture type */
    picture.type = unknownPict;
    picture.width = 0;
    picture.height = 0;
    picture.goalWidth = 0;
    picture.goalHeight = 0;
    picture.scaleX = 100;
    picture.scaleY = 100;
    strcpy(picture.name, "");
}

/* 
 * This function reads in a picture 
 * 
 * {{\NeXTGraphic build.tiff \width740 \height740 \noorient}�}
 *
 */
static void ReadNextGraphic(void)
{
    char filename[100], buff[100];
    int i=0;
    int width=0;
    int height=0;

    requireGraphicxPackage = true;

    /* skip to first letter of file name */
    RTFGetToken();
    while (rtfClass == rtfText && rtfMajor == ' ')
        RTFGetToken();  

    /* read the file name */
    i=0;
    while (rtfClass == rtfText && rtfMajor != ' ' && i < 100) {
        filename[i++] = rtfMajor;
        RTFGetToken();  
    }
    filename[i] = '\0';

    while (!RTFCheckCM(rtfGroup, rtfEndGroup)) {
        switch (rtfMinor) {
        case rtfNeXTGHeight:
            height = rtfParam/20;
            break;
        case rtfNeXTGWidth:
            width = rtfParam/20;
            break;
        default:
            break;
        }
        RTFGetToken();
    }
    
    /* skip everything until outer brace */
    SkipGroup();

    PutLitStr("\\includegraphics");
    if (width || height) {
        PutLitStr("[");
        if (width) {
            snprintf(buff, 100, "width=%dpt, ", width);
            PutLitStr(buff);
        }
        if (height) {
            snprintf(buff, 100, "height=%dpt", height);
            PutLitStr(buff);
        }
        PutLitStr("]");
    }
    PutLitStr("{");
    PutLitStr(filename);
    PutLitStr("}\n");
}

/*
 * slow simplistic reimplementation of strcasestr for systems that
 * don't include it in their library
 *
 * based on a GPL implementation in OpenTTD found under GPL v2
 */

char *my_strcasestr(const char *haystack, const char *needle)
{
    size_t hay_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    while (hay_len >= needle_len) {
        if (strncasecmp(haystack, needle, needle_len) == 0)
            return (char *) haystack;

        haystack++;
        hay_len--;
    }

    return NULL;
}

static void ReadObjWidth(void)
{
    g_object_width = rtfParam;
}


/*
* parses \objectclass and adds the class type to the global variable 'object'
*/
static void GetObjectClass(int *groupCounter)
{
    int reachedObjectClass = 0;
    int reachedEndGroup = 0;
    int i;

/* keep scanning until \objectclass is found */
    while (!reachedObjectClass) {
        RTFGetToken();
        if (RTFCheckMM(rtfObjAttr, rtfObjWid)!=0)
            ReadObjWidth();
        if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0)
            (*groupCounter)++;
        else if (RTFCheckCM(rtfGroup, rtfEndGroup) != 0)
            (*groupCounter)--;
        if (RTFCheckMM(rtfDestination, rtfObjClass) != 0)
            reachedObjectClass = 1;
        if (*groupCounter == 0) {
            object.class = unknownObjClass;
            return;
        }
    }

/* read the object class */
    strcpy(object.className, "");
    while (!reachedEndGroup) {
        RTFGetToken();
        if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0)
            RTFSkipGroup();
        if (RTFCheckCM(rtfGroup, rtfEndGroup) == 0)
            strcat(object.className, rtfTextBuf);
        else {
            reachedEndGroup = 1;
            (*groupCounter)--;
        }
    }

/* do we recognize this object class? */
    for (i = 0; objectClassList[i] != NULL; i++) {
        if (my_strcasestr(object.className, objectClassList[i]) != NULL) {
            object.class = i;
            break;
        }
        object.class = 0;
    }
}


/*
 * The result section of an \object usually contains a picture of the object
 */
static int ReachedResult(int *groupCount)
{
    RTFGetToken();

    if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0) {
        (*groupCount)++;
        return (0);
    }

    if (RTFCheckCM(rtfGroup, rtfEndGroup) != 0) {
        (*groupCount)--;
        return (0);
    }

    if (RTFCheckMM(rtfDestination, rtfObjResult) != 0 ||
        RTFCheckMM(rtfWord97ObjAttr, rtfWord97ObjResult) != 0 ||
        RTFCheckMM(rtfDestination, rtfPict) != 0 ||
        RTFCheckMM(rtfWord97ObjAttr, rtfWord97ObjText) != 0) {
        if (RTFCheckMM(rtfDestination, rtfPict) != 0)
            word97ObjectType = word97Picture;
        else if (RTFCheckMM(rtfDestination, rtfObjResult) != 0)
            word97ObjectType = standardObject;
        else if (RTFCheckMM(rtfWord97ObjAttr, rtfWord97ObjResult) != 0)
            word97ObjectType = word97Object;
        else if (RTFCheckMM(rtfWord97ObjAttr, rtfWord97ObjText) != 0)
            word97ObjectType = word97ObjText;
        (*groupCount)--;        /* account for opening brace just
                                 * before result control word */
        return (1);
    }

    if (rtfClass == rtfEOF) {
        RTFPanic("* EOF reached!\n");
        exit(1);
    }

    return (0);
}

/*
 * Decodes the OLE and extract the specified stream type into a buffer.
 * This function uses the cole library
 */
static int
DecodeOLE(char *objectFileName, char *streamType,
          unsigned char **nativeStream, uint32_t * size)
{
    COLEFS *cfs;
    COLERRNO colerrno;
    COLEFILE *coleFile;

    cfs = cole_mount(objectFileName, &colerrno);
    if (cfs == NULL) {
        cole_perror("DecodeOLE cole_mount", colerrno, objectFileName);
        return (1);
    }

#ifdef COLE_VERBOSE
    cole_print_tree (cfs, &colerrno);
#endif

    if ((coleFile = cole_fopen(cfs, streamType, &colerrno)) == NULL) {
        cole_perror("DecodeOLE cole_fopen", colerrno, objectFileName);
        cole_umount(cfs, NULL);
        return 1;
    }

    *size = (uint32_t) cole_fsize(coleFile);

    *nativeStream = (unsigned char *) malloc(*size);

    if (*nativeStream == NULL) {
        RTFMsg("* DecodeOLE: memory allocation failed for native stream!\n");
        cole_fclose(coleFile, &colerrno);
        cole_umount(cfs, NULL);
        return 1;
    }

    if (cole_fread(coleFile, *nativeStream, *size, &colerrno) == 0) {
        cole_perror("DecodeOLE cole_fread", colerrno, objectFileName);
        cole_fclose(coleFile, &colerrno);
        cole_umount(cfs, NULL);
        free(nativeStream);
        return 1;
    }

    if (cole_fclose(coleFile, &colerrno) != 0) {
        cole_perror("DecodeOLE cole_fclose", colerrno, objectFileName);
        cole_umount(cfs, NULL);
        free(nativeStream);
        return 1;
    }

    if (cole_umount(cfs, &colerrno)) {
        cole_perror("DecodeOLE cole_umount", colerrno, objectFileName);
        free(nativeStream);
        return (1);
    }
    return 0;
}

static int ishex(char c)
{
    if ( '0' <= c && c <= '9' )
        return 1;
    if ('a' <= c && c <= 'f')
        return 1;
    if ('A' <= c && c <= 'F')
        return 1;
    return 0;
}

/*
 * Save the hex-encoded object data and as binary bytes in objectFileName
 */
static void ReadObjectData(char *objectFileName, int type, int offset)
{
    char dummyBuf[20];
    char *OLE_MARK = "d0cf11e0";
    FILE *objFile;
    int i;
    uint8_t hexNumber;
    uint8_t hexEvenOdd = 0;       /* should be even at the end */

    if (type == EquationClass) {
        (oleEquation.count)++;
        snprintf(dummyBuf, 20, "-eqn%03d.eqn", oleEquation.count);
    } else
        snprintf(dummyBuf, 20, ".obj");

    /* construct full path of file name (without .tex) */
    strcpy(objectFileName, RTFGetOutputName());
    objectFileName[strlen(objectFileName)-4]='\0';
    strcat(objectFileName, dummyBuf);

    /* open object file */
    objFile = fopen(objectFileName, "wb");
    if (!objFile)
        RTFPanic("Cannot open input file %s\n", objectFileName);

/*
 * The offset to the data should be a constant, but it seems to
 * vary from one RTF file to the next.  Perhaps there is some
 * Microsoft documentation that explains how many bytes exactly
 * there is before we get to the chewy nuguat.  After adding
 * several hacks to shift and squirm, the simplest thing is just
 * to skip to a sequence of bytes that should start each OLE
 * object.  This is what is done now.
 */

    i = 0;
    while (i < 8) {
        RTFGetToken();
        if (rtfTextBuf[0] == 0x0a || rtfTextBuf[0] == 0x0d)
            continue;

        if (rtfTextBuf[0] == OLE_MARK[i])
            i++;
        else if (rtfTextBuf[0] == OLE_MARK[0])
            i = 1;
        else if (ishex(rtfTextBuf[0]))
            i = 0;
        else
            break;
    }

    if (i<8) {
        RTFMsg("* OLE object does not have proper header\n");
        fclose(objFile);
        return;
    }

    fputc(0xd0, objFile);
    fputc(0xcf, objFile);
    fputc(0x11, objFile);
    fputc(0xe0, objFile);

    /* each byte is encoded as two hex chars ... ff, a1, 4c, ...*/
    while (1) {
        RTFGetToken();

        /* CR or LF in the hex stream should be skipped */
        if (rtfTextBuf[0] == 0x0a || rtfTextBuf[0] == 0x0d)
            RTFGetToken();

        if (rtfClass == rtfGroup)
            break;

        hexNumber = 16 * RTFCharToHex(rtfTextBuf[0]);
        hexEvenOdd++;

        RTFGetToken();

        if (rtfTextBuf[0] == 0x0a || rtfTextBuf[0] == 0x0d)
            RTFGetToken();  /* should not happen */

        if (rtfClass == rtfGroup)
            break;

        hexNumber += RTFCharToHex(rtfTextBuf[0]);   /* this is the the number */
        hexEvenOdd--;
        fputc(hexNumber, objFile);
    }

    if (fclose(objFile) != 0)
        printf("* error closing object file %s\n", objectFileName);

    if (hexEvenOdd)
        printf ("* Warning! Odd number of hex characters read for object!\n");
}

/*
 * After an equation look for an equation number
 */
char * EqnNumberString(void)
{
    char theNumber[10], comma[2], *s, *t;
	int index=0;
	
	theNumber[0]='\0';
	comma[0]='\0';
    /* skip to text following equation, stop looking a \par or \pard  */
    do  {
    	RTFGetToken();

		/* paragraph ended ==> no equation number */
    	if (RTFCheckCMM(rtfControl, rtfSpecialChar, rtfPar)) {
    		RTFUngetToken();
    		return NULL;
        }
    	
		/* don't emit any tabs */
    	if (RTFCheckCMM(rtfControl, rtfSpecialChar, rtfTab))
    		continue;

		/* don't emit pictures */
    	if (RTFCheckCMM(rtfControl, rtfDestination, rtfPict)) {
    		SkipGroup();
    		continue;
    	}

        if (rtfClass == rtfText) {
        	/* don't stop for spaces */
        	if (isspace(rtfTextBuf[0])) 
        		continue;

			/* commas or periods are common punctuation following an equation
			   so keep them but keep looking for a real equation number */
        	if (rtfTextBuf[0] == ',' || rtfTextBuf[0] == '.') {
            	comma[0]=rtfTextBuf[0];
            	comma[1]='\0';
        		continue;
        	}
        	
        	/* found a text character that might start an equation number*/
        	break;
        }

    	RTFRouteToken();
    	
	} while (rtfClass != rtfEOF);
    
	/* collect the equation number */
    do {
    	if (RTFCheckCMM(rtfControl, rtfSpecialChar, rtfPar)) {
    		RTFUngetToken();
    		break;;
        }
        
		if (rtfClass == rtfText) {
			char c=rtfTextBuf[0];

			/* eqn numbers must start with '(', '[', or a digit */
			if (index==0 && !(isdigit(c) || c == '(' || c == ']') ) break;
						
			theNumber[index]=c;
			index++;
			if (c==')' || c==']') break;
		}
		
		RTFGetToken();
	} while (rtfClass != rtfEOF && index<10);

	if (index == 0) {
    	RTFUngetToken();
    	return strdup(comma);
    }

	theNumber[index]='\0';
	s=strdup_together(comma,"\\eqno");
	t=strdup_together(s,theNumber);
	free(s);

    return t;	
}

/*
 * Convert OLE file containing equation
 */

boolean ConvertEquationFile(char *objectFileName)
{
    unsigned char *nativeStream;
    char *EqNo;
    MTEquation *theEquation;
    uint32_t equationSize;

    nativeStream = NULL;
    theEquation = NULL;

    /* Decode the OLE and extract the equation stream into buffer nativeStream */
    if (DecodeOLE(objectFileName, "/Equation Native", &nativeStream, &equationSize)) {
        RTFMsg("* error decoding OLE equation object!\n");
        return (false);
    }

    theEquation = (MTEquation *) malloc(sizeof(MTEquation));
    if (theEquation == NULL) {
        RTFMsg("* error allocating memory for equation!\n");
        free(nativeStream);
        return (false);
    }

   /* __cole_dump(nativeStream+slop, nativeStream+slop, 64, NULL); */
    if (*(nativeStream) == 0x1c && *(nativeStream+1) == 0x00) {
        equationSize -= MTEF_HEADER_SIZE;

        if (!Eqn_Create(theEquation, nativeStream+MTEF_HEADER_SIZE, equationSize)) {
            RTFMsg("* could not create equation structure!\n");
            free(nativeStream);
            free(theEquation);
            return (false);
        }

        if (nowBetweenParagraphs) {
            theEquation->m_inline = 0;
            EndParagraph();
            NewParagraph();
        	EqNo=EqnNumberString();
        } else {
            theEquation->m_inline = 1;
            EqNo=NULL;
        }

        if (g_eqn_insert_name) {
            PutLitStr("\\fbox{file://");
            PutEscapedLitStr(objectFileName);
            PutLitStr("}");
            requireHyperrefPackage = true;
        }

        /* this actually writes the equation */
        Eqn_TranslateObjectList(theEquation, ostream, 0);
        PutLitStr(theEquation->m_latex_start);
        PutLitStr(theEquation->m_latex);
        PutLitStr(EqNo);
        PutLitStr(theEquation->m_latex_end);
        Eqn_Destroy(theEquation);
    }

    if (theEquation != NULL)
        free(theEquation);

    if (nativeStream != NULL)
        free(nativeStream);

    requireAmsSymbPackage = true;
    requireAmsMathPackage = true;
    return true;
}

/*
 * Translate an object containing a MathType equation
 */
static boolean ReadEquation(int *groupCount)
{
    boolean result;
    char objectFileName[rtfBufSiz];

    /* look for start of \objdata  group */
    while (!RTFCheckMM(rtfDestination, rtfObjData)) {

        RTFGetToken();

        if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0)
            (*groupCount)++;

        else if (RTFCheckCM(rtfGroup, rtfEndGroup) != 0) {
            (*groupCount)--;
            if (*groupCount == 0) {
                RTFMsg("* ReadEquation: objdata group not found!\n");
                return (false);
            }

        } else if (rtfClass == rtfEOF) {
            RTFPanic("* ReadEquation: EOF reached!\n");
            exit(1);
        }
    }

    /* save hex-encoded object data as a binary objectFileName */
    ReadObjectData(objectFileName, EquationClass, EQUATION_OFFSET);
    (*groupCount)--;

    result = ConvertEquationFile(objectFileName);

    if (!g_eqn_keep_file)
        remove(objectFileName);

    return result;
}


/*
 * Read and process \object token
 */
static void ReadObject(void)
{
    int i;
    int groupCounter = 1;       /* one opening brace has been counted */
    int temp;
    boolean res;
    /* char *fn = "ReadObject"; */
    /*  RTFMsg("%s: * starting ...\n", fn); */

    GetObjectClass(&groupCounter);

    switch (object.class) {
    case unknownObjClass:
    default:
        /* RTFMsg("%s: * unsupported object '%s', skipping...\n", fn, object.className); */
        RTFSkipGroup();
        break;

    case EquationClass:
        /* RTFMsg("%s: * equation object '%s', processing...\n", fn, object.className); */

        if (prefs[pConvertEquation]) {
            res = ReadEquation(&groupCounter);
            if (!res) fprintf(stderr, "failed to convert equation\n");
        } else
            res = false;


        /* if unsuccessful, include the equation as a picture */
        if (!res || g_eqn_insert_image) {
            temp = groupCounter;

            while (!RTFCheckMM(rtfDestination, rtfPict)) {
                RTFGetToken();
                if (RTFCheckCM(rtfGroup, rtfBeginGroup))
                    groupCounter++;
                if (RTFCheckCM(rtfGroup, rtfEndGroup))
                    groupCounter--;
                if (groupCounter < temp)
                    break;
            }

            if (groupCounter > temp) {
                ReadPicture();
                if (groupCounter - 1 - temp > 0) {
                    for (i = 0; i < groupCounter - 1 - temp; i++)
                        RTFSkipGroup();
                }
            }
            groupCounter = temp;
        }
        break;

    case WordPictureClass:
    case MSGraphChartClass:
        while (!ReachedResult(&groupCounter));
        ReadPicture();
        break;
    }

    object.class = 0;
    strcpy(object.className, "");

    /* if there are open groups left, close them */
    if (groupCounter != 0) {
        for (i = 0; i < groupCounter; i++)
            RTFSkipGroup();
    }

    /* send the last closing brace back into the router */
    RTFRouteToken();
}


/* This is the result field of the Word97 object */
static void ReadWord97Result(void)
{
    int i;
    int groupCount = 1;         /* one opening brace has been counted */
/*     char *fn = "ReadWord97Result"; */
/*     RTFMsg("%s: starting ...\n",fn); */

    /* scan until object or picture is reached */
    while (groupCount != 0) {
        RTFGetToken();
        if (RTFCheckMM(rtfDestination, rtfObject) != 0) {
            ReadObject();
            groupCount--;
            break;
        } else if (RTFCheckMM(rtfDestination, rtfPict) != 0) {
            ReadPicture();
            groupCount--;
            break;
        } else if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0)
            groupCount++;
        else if (RTFCheckCM(rtfGroup, rtfEndGroup) != 0)
            groupCount--;
    }

    if (groupCount == 0)
        printf
            ("* Warning: no supported structure in Word97 object found.\n");


    /* if there are open groups left, skip to the end */
    if (groupCount > 0)
        for (i = 0; i < groupCount; i++)
            RTFSkipGroup();

    RTFRouteToken();

}


/* Of course, Word97 has to do everything differently. */
static void ReadWord97Object(void)
{
    int i;
    long objectStart;
    int groupCount = 1;         /* one opening brace has been counted */
    int word97ObjTextGL = 1;
    short prevChar;
/*     char *fn = "ReadWord97Object"; */
/*     RTFMsg("%s: starting ...\n",fn); */

    word97ObjectType = unknownWord97Object;

    /* look for a standard embedded object first: may have an equation */
    /* mark the current cursor position */
    prevChar = RTFPushedChar();
    RTFStoreStack();
    objectStart = ftell(ifp);

    while (!RTFCheckMM(rtfDestination, rtfObject)) {
        RTFGetToken();

        if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0)
            groupCount++;

        else if (RTFCheckCM(rtfGroup, rtfEndGroup) != 0) {
            groupCount--;
            /* did not find a standard object */
            if (groupCount == 0) {
                fseek(ifp, objectStart, 0);
                RTFSimpleInit();
                RTFSetPushedChar(prevChar);
                RTFRestoreStack();
                break;
            }
        }

    }

    /* if we found a standard object, read it and get out */
    if (RTFCheckMM(rtfDestination, rtfObject)) {
        ReadObject();
        groupCount--;
        /* if there are open groups left, close them */
        if (groupCount != 0)
            for (i = 0; i < groupCount; i++)
                RTFSkipGroup();
        /* send the last closing brace back into the router */
        RTFRouteToken();
        return;
    }

    groupCount = 1;

    while (!ReachedResult(&groupCount)) {
        if (groupCount == 0) {
            RTFMsg("* unknown Word97 object...\n");
/*             PutLitStr(" [ missing object here ] "); */
            RTFRouteToken();
            return;
        }
    }

    switch (word97ObjectType) {
    case word97Picture:
        ReadPicture();
        break;
    case standardObject:
    case word97Object:
        ReadWord97Result();
        break;
    case word97ObjText:
        if (!insideTable) {
            StopTextStyle();
            word97ObjTextGL = braceLevel;
            while (braceLevel && braceLevel >= word97ObjTextGL) {
                RTFGetToken();
                RTFRouteToken();
            }
        }
        break;
    }

    if (groupCount == 0)
        return;

    /* if there are open groups left, close them */
    if (groupCount != 0)
        for (i = 0; i < groupCount; i++)
            RTFSkipGroup();

    object.word97 = 0;

    /* send the last closing brace back into the router */
    RTFRouteToken();
}

static void ReadUnicode(void)
{
	int thechar;
	
	if (rtfParam<0)
		thechar = rtfParam + 65536;
	else	
		thechar = rtfParam;
	
	/* \uNNNNY, drop Y is a default unicode char */
	if (rtfMinor == rtfUnicode)
		RTFGetToken();
		
	PrepareForChar();

//	fprintf(stderr, "Unicode --- %d, 0x%04X\n", thechar, thechar);
    if (thechar == 8212) {
        PutLitStr("---");
        return;
    }

    if (thechar == 8216) {
        PutLitStr("`");
        return;
    }

    if (thechar == 8217) {
        PutLitStr("'");
        return;
    }

    if (thechar == 8220) {
        PutLitStr("``");
        return;
    }

    if (thechar == 8221) {
        PutLitStr("''");
        return;
    }

    if (thechar == 8230) {
        PutLitStr("...");
        return;
    }

    if (0xC0 <= thechar && thechar <=0xFF) {
        PutLitChar(thechar);
        return;
    }

    /* directly translate greek */
    if (913 <= thechar && thechar <= 969) {
        PutMathLitStr(UnicodeGreekToLatex[thechar-913]);
        return;
    }

    /* and also a bunch of wierd codepoints from the Symbol font
       that end up in a private code area of Unicode */
    if (61472 <= thechar && thechar <= 61632) {
        PutMathLitStr(UnicodeSymbolFontToLatex[thechar-61472]);
        return;
    }

    PutIntAsUtf8(thechar);
    /*snprintf(unitext,20,"\\unichar{%d}",(int)thechar);
    if (0) fprintf(stderr,"unicode --- %s!\n",unitext);
    PutLitStr(unitext);
    */
}

static void SkipFieldResult(void)
{
    /* skip over to the result group */
    while (!RTFCheckCMM(rtfControl, rtfDestination, rtfFieldResult))
        RTFGetToken();

    RTFSkipGroup();
    RTFRouteToken();
}

static void ReadHyperlink(void)
{
    int localGL;

    PutLitStr("\\href{");

    localGL = braceLevel;

    insideHyperlink = true;

    while (braceLevel && braceLevel >= localGL) {
        RTFGetToken();
        if (rtfClass == rtfText) {
            if (rtfTextBuf[0] != '"'
                && !RTFCheckMM(rtfSpecialChar, rtfLDblQuote)
                && !RTFCheckMM(rtfSpecialChar, rtfRDblQuote))
                    RTFRouteToken();
         } else
             RTFRouteToken();
    }

    PutLitStr("}{");

    /* skip over to the result group */
    while (!RTFCheckCMM(rtfControl, rtfDestination, rtfFieldResult))
        RTFGetToken();

    localGL = braceLevel;
    /* switch off hyperlink flag */
    insideHyperlink = false;

    while (braceLevel && braceLevel >= localGL) {
        RTFGetToken();
        RTFRouteToken();
    }

    PutLitStr("}");
    requireHyperrefPackage = true;
}

static void ReadSymbolField(void)
{
    char buf[100];
    short major, minor;
    short *currentCharCode = curCharCode;

    /* go to the start of the symbol representation */
    if (RTFGetToken() != rtfText) {
        if (RTFCheckCM(rtfGroup, rtfBeginGroup) != 0)
            RTFSkipGroup();
        RTFSkipGroup();
        RTFRouteToken();
        return;
    }

    /* read in the symbol token */
    strcpy(buf, rtfTextBuf);
    while (strcmp(rtfTextBuf, " ") != 0) {
        RTFGetToken();
        if (strcmp(rtfTextBuf, " ") != 0)
            strcat(buf, rtfTextBuf);
    }

    /* convert the text symbol token to an int */
    major = atoi(buf);
    
    /* do the mapping */
    curCharCode = symCharCode;
    minor = RTFMapChar(major);
    curCharCode = currentCharCode;

    /* set the rtf token to the new value */
    RTFSetToken(rtfText, major, minor, rtfNoParam, buf);

    if (minor >= rtfSC_therefore && minor < rtfSC_currency)
        requireAmsSymbPackage = true;

    /* call the handler for text */
    TextClass();

    RTFSkipGroup();
    RTFRouteToken();
}

/* the following streams should just emit ... HToc268803753
 *    PAGEREF _Toc268803753 \\h
 *    HYPERLINK \\l "_Toc268803753"
 *     _Toc268803753
 */
static void emitBookmark(void)
{
    int started = 0;

    RTFGetToken();
    while (rtfClass == rtfText) {
        switch (rtfTextBuf[0]) {
        case ' ':
            if (started) {     /* assume that bookmarks optionally start and end with spaces */
                RTFSkipGroup();
                return;
            }
            break;
        case '"':
            break;
        case '\\':
            RTFGetToken(); /* drop backslash and the next letter */
            break;
        case '_':
            started = 1;
            PutLitStr("H");
            break;
        default:
            started = 1;
            PutLitStr(rtfTextBuf);
            break;
        }
        RTFGetToken();
    }
}

/*
 *  Just emit \pageref{HToc268612944} for {\*\fldinst {...  PAGEREF _Toc268612944 \\h } ..}
*/
static void ReadPageRefField(void)
{
    PutLitStr("\\pageref{");
    emitBookmark();
    PutLitStr("}");
    RTFRouteToken();
    
    SkipFieldResult();
}

/*
 *  Just emit \pageref{HToc268612944} for {\*\fldinst {...  PAGEREF _Toc268612944 \\h } ..}
*/
static void ReadPageField(void)
{
    PutLitStr("\\thepage{}");    
    SkipFieldResult();
}

/*
 *  Three possible types of fields
 *     (1) supported ... translate and ignore FieldResult
 *     (2) tolerated ... ignore FieldInst and translate FieldResult
 *     (3) unknown   ... ignore both FieldInst and FieldResult
 */
static void ReadFieldInst(void)
{
    char buf[100];
    int i;
    int groupCount = 1;

    /* skip to text identifying the type of FIELD  */
    while (rtfClass != rtfText || rtfMinor == rtfSC_space) {
        RTFGetToken();
        if (RTFCheckCM(rtfGroup, rtfBeginGroup))
            groupCount++;
        if (RTFCheckCM(rtfGroup, rtfEndGroup))
            groupCount--;
        if (groupCount == 0) {
            RTFRouteToken();
            return;
        }
    }

    /* extract text identifying the FIELD into buf */
    strcpy(buf, rtfTextBuf);
    while (strcmp(rtfTextBuf, " ") != 0 && rtfClass != rtfGroup) {
        RTFGetToken();
        if (strcmp(rtfTextBuf, " ") != 0 && rtfClass != rtfGroup)
            strcat(buf, rtfTextBuf);
    }
/*    RTFMsg("%s: FIELD type is %s\n",fn,buf);*/

    if (prefs[pConvertHypertext] && strcmp(buf, "HYPERLINK") == 0 ) {
        ReadHyperlink();
        return;
    }

    if (strcmp(buf, "SYMBOL") == 0) {
        ReadSymbolField();
        return;
    }

    if (strcmp(buf, "PAGEREF") == 0) {
        ReadPageRefField();
        return;
    }

    if (strcmp(buf, "PAGE") == 0) {
    	ReadPageField();
        return;
    }

    /* Unsupported FIELD type ... the best we can do is bail from rtfFieldInst
       and hope rtfFieldResult can be processed  */
    for (i = 0; i < groupCount; i++)
        RTFSkipGroup();
}

/*
 *  Just emit \label{HToc268612944} for {\*\bkmkstart _Toc268612944}
 */
static void ReadBookmarkStart(void)
{
    PutLitStr("\\label{");
    emitBookmark();
    PutLitStr("}");
    RTFRouteToken();
}

static void HandleOptionalTokens(void)
{
    RTFGetToken();
    
    switch (rtfMinor) {
    case rtfBookmarkStart:
        ReadBookmarkStart();
        break;
        
    case rtfFieldInst:
        ReadFieldInst();
        break;
        
    case rtfWord97Picture:
        /* expecting {\*\shppict{\pict{...}}{\nonshppict{\pict{...}} */
        
        RTFGetToken();
        if (rtfClass != rtfGroup) { SkipGroup(); break;}
        
        RTFGetToken();    /* should be pict */
        if (rtfMinor != rtfPict){ SkipGroup(); break;}
        
        RTFRouteToken();  /* handle pict */
        RTFGetToken();    /* should be } */
        RTFRouteToken();  /* handle last brace from shppict */
        RTFGetToken();    /* should be { */
        if (rtfClass != rtfGroup) { SkipGroup(); break;}
        
        RTFGetToken();    /* should be nonshppict */
        SkipGroup();      /* because we don't want two pictures in latex file */
        break;
        
    default:
    //  ExamineToken("HandleOptionalTokesn"); 
        RTFSkipGroup();
        break;
    }
}

/*
 * The reason these use the rtfSC_xxx thingies instead of just writing
 * out ' ', '-', '"', etc., is so that the mapping for these characters
 * can be controlled by the latex-encoding file.
 */
static void SpecialChar(void)
{
    switch (rtfMinor) {
    case rtfSect:
    case rtfLine:
    case rtfPar:
        if (nowBetweenParagraphs)
            current_vspace += abs(paragraph.lineSpacing);
        nowBetweenParagraphs = true;
        break;
    case rtfNoBrkSpace:
        if (nowBetweenParagraphs)
            paragraph.extraIndent += 0;
        else
            PutStdChar(rtfSC_nobrkspace);
        break;
    case rtfTab:
        if (nowBetweenParagraphs)
            paragraph.extraIndent += 360;
        else if (prefs[pConvertTextNoTab])
            PutLitChar(' ');
        else
            PutLitStr("\\tab ");
        break;
    case rtfNoBrkHyphen:
        PutStdChar(rtfSC_nobrkhyphen);
        break;
    case rtfBullet:
        PutStdChar(rtfSC_bullet);
        break;
    case rtfEmDash:
        PutLitStr("---");
        break;
    case rtfEnDash:
        PutLitStr("-");
        break;
    case rtfLQuote:
        PutLitStr("`");
        break;
    case rtfRQuote:
        PutLitStr("'");
        break;
    case rtfLDblQuote:
        PutLitStr("``");
        break;
    case rtfRDblQuote:
        PutLitStr("''");
        break;
    case rtfPage:
        if (!insideTable)
            PutLitStr("\\pagebreak{}");
        break;
    case rtfOptDest:
        HandleOptionalTokens();
        break;
    case rtfCurHeadPage:
        PutLitStr("\\thepage{}");
        break;
    case rtfCurFNote:
        break;
    default:
        ExamineToken("SpecialChar");  /* comment out before release */
    }
}

static void DoHeaderFooter(void)
{
	char *buff, *s, *option;
	size_t hfStartPos, hfEndPos, len;
	int isHeader, isFirst;
	int level = braceLevel;
	
	if (insideHeaderFooter) return;
	
	insideHeaderFooter = true;
	suppressLineBreak = true;
	isHeader = (rtfMinor == rtfHeader) || (rtfMinor == rtfHeaderFirst) ;
	isFirst  = (rtfMinor == rtfHeaderFirst) || (rtfMinor == rtfFooterFirst) ;
	
    StopTextStyle();

    hfStartPos = ftell(ofp);

	switch (rtfMinor) {
	case rtfHeader:
		option = "\\lhead{";
		break;
	case rtfHeaderRight:
		option = "\\fancyhead[LO]{";
		break;
	case rtfHeaderLeft:
		option = "\\fancyhead[LE]{";
		break;
	case rtfHeaderFirst:
		option = "  \\lhead{";
		break;
	case rtfFooter:
		option = "\\lfoot{";
		break;
	case rtfFooterRight:
		option = "\\fancyfoot[LO]{";
		break;
	case rtfFooterLeft:
		option = "\\fancyfoot[LE]{";
		break;
	case rtfFooterFirst:
		option = "  \\lfoot{";
		break;
	}

	PutLitStr(option);	
    while (braceLevel && braceLevel >= level) {
        RTFGetToken();
    	RTFRouteToken();
    }
    StopTextStyle();
    PutLitStr("}\n");
    
    /* copy header/footer  */
    hfEndPos = ftell(ofp);
    len = hfEndPos - hfStartPos;
    
    /* Don't bother unless the header contains something */
    /* and we are still in the preamble */
    
    if (len<=strlen(option)+2) {
    	/* erase empty command */
		fseek(ofp, hfStartPos, 0);

	} else {

		/* save only if still in preamble or it is a first page command */
		if (isFirst || !wroteBeginDocument) {
			requireFancyHdrPackage = true;
			buff = malloc(len+1);
			fseek(ofp, hfStartPos, 0);
			fread(buff,1,len,ofp);
			buff[len] = '\0';
			
			if (!isFirst) {
				s = strdup_together(preambleFancyHeader,buff);
				if (preambleFancyHeader) free(preambleFancyHeader);
				preambleFancyHeader = s;
			} else {
				s = strdup_together(preambleFancyHeaderFirst,buff);
				if (preambleFancyHeaderFirst) free(preambleFancyHeaderFirst);
				preambleFancyHeaderFirst = s;
			}
			fseek(ofp, hfStartPos, 0);
		}
    }
    
    suppressLineBreak = false;
    insideHeaderFooter = false;
}

/*
 * This function notices destinations that should be ignored
 * and skips to their ends.  This keeps, for instance, picture
 * data from being considered as plain text.
 */

static void Destination(void)
{
    switch (rtfMinor) {
    case rtfFNContSep:
    case rtfFNContNotice:
    case rtfInfo:
    case rtfIndexRange:
    case rtfITitle:
    case rtfISubject:
    case rtfIAuthor:
    case rtfIOperator:
    case rtfIKeywords:
    case rtfIComment:
    case rtfIVersion:
    case rtfIDoccomm:
    case rtfUserPropsGroup:
    case rtfWGRFmtFilter:
        RTFSkipGroup();
        break;
    case rtfNeXTGraphic:
        ReadNextGraphic();
        break;
    case rtfUnicode:
    case rtfUnicodeFake:
        ReadUnicode();
        break;
    case rtfHeaderLeft:
    case rtfHeaderRight:
    case rtfFooterLeft:
    case rtfFooterRight:
    case rtfFooterFirst:
    case rtfHeaderFirst:
    case rtfHeader:
    case rtfFooter:
        DoHeaderFooter();
        break;
    }
}

/* 
 * In RTF, the code page is specified in at least three different places
 * 
 * (1) as the third token in the file, e.g., {\rtf1\ansi
 * (2) in the font table for each font e.g., \fcharset2
 * (3) by the code page token, e.g., \ansicpg1252
 *
 * A pragmatic approach is used here.  The third token is used to
 * set genCharCode.  If \aniscpg is found, then genCharCode will be
 * changed to that.  Here we just change if it is the symbol font.
 */
static void RTFSetGenCharSet(void)
{   
    switch(rtfMinor) {
    case rtfAnsiCharSet:
        genCharCode = cp1252CharCode;
        break;
    case rtfMacCharSet:
        genCharCode = cpMacCharCode;
        break;
    case rtfPcCharSet:
        genCharCode = cp437CharCode;
        break;
    case rtfPcaCharSet:
        genCharCode = cp850CharCode;
        break;
    }
        
    /* check for the \ansicpg control word */
    RTFPeekToken();
    if (RTFCheckCMM(rtfControl, rtfFontAttr, rtfAnsiCodePage)) {  /* we will handle the token */
        RTFGetToken(); 
        switch (rtfParam) {
            case 437:
                genCharCode=cp437CharCode;
                break;
            case 850:
                genCharCode=cp850CharCode;
                break;
            case 1250:
                genCharCode=cp1250CharCode;
                break;
            case 1251:
                genCharCode=cp1251CharCode;
                break;
            case 1252:
                genCharCode=cp1252CharCode;
                break;
            case 1254:
                genCharCode=cp1254CharCode;
                break;
            case 10000:
                genCharCode=cpMacCharCode;
                break;
            case 10001:
                genCharCode=cp932CharCode;
                break;
            case 10008:
                genCharCode=cp936CharCode;
                break;
        }
    }
    
    curCharCode = genCharCode;
} 

/* decides what to do when a control word is encountered */
static void ControlClass(void)
{
    switch (rtfMajor) {
    case rtfDefFont:
        RTFSetDefaultFont(rtfParam);
    case rtfFontAttr:
        switch (rtfMinor) {
        case rtfAnsiCodePage:
        case rtfFontCodePage:
            /* codePage = rtfParam;*/
            break;
        }
        break;
    case rtfDestination:
        Destination();
        break;
    case rtfSpecialChar:
        SpecialChar();
        break;
    case rtfCharAttr:
        SetTextStyle();
        break;
    case rtfListAttr:
        RTFSkipGroup();
        break;
    case rtfTblAttr:            /* trigger for reading table */
        if (rtfMinor == rtfRowDef && !(insideTable)) {
            RTFUngetToken();
            DoTable();          /* if we are not already inside a table, get into it */
        } else
            DoTableAttr();      /* if we are already inside
                                 * a table, set table attributes */
        break;
    case rtfParAttr:
        ParAttr();
        break;
    case rtfSectAttr:
        SectAttr();
        break;
    case rtfWord97ObjAttr:
        if (rtfMinor == rtfShapeName || rtfMinor == rtfShapeValue)
            SkipGroup();
        break;
    case rtfCharSet:
        RTFSetGenCharSet();
        break;
    }

    /* handles {\*\keyword ...} */
//  if (RTFCheckMM(rtfSpecialChar, rtfOptDest))
//      RTFSkipGroup();

}

/*
 * Prepares output TeX file for each input RTF file.
 * Sets globals and installs callbacks.
 */
int BeginLaTeXFile(void)
{
    int i;
    
    RTFSetDefaultFont(-1);
    insideFootnote = false;
    insideHyperlink = false;
    insideTable = false;
    insideHeaderFooter = false;
    section.newStyle = false;
    section.cols = 1;

    requireSetspacePackage = false;
    requireTablePackage = false;
    requireGraphicxPackage = false;
    requireAmsSymbPackage = false;
    requireMultiColPackage = false;
    requireUlemPackage = false;
    requireFixLtx2ePackage = false;
    requireHyperrefPackage = false;
    requireMultirowPackage = false;
    requireAmsMathPackage = false;
    requireFancyHdrPackage = false;

	preambleFancyHeader=NULL;
	preambleFancyHeaderFirst=NULL;

    picture.count = 0;
    picture.type = unknownPict;
    oleEquation.count = 0;
    object.class = unknownObjClass;
    object.word97 = 0;
    table.cellCount = 0;
    table.cellInfo = NULL;
    table.cellMergePar = none;
    table.multiCol = false;
    table.multiRow = false;
    
    InitTextStyle();
    textStyleWritten = textStyle;
    
    InitParagraphStyle();
    paragraphWritten = paragraph;
    nowBetweenParagraphs = true;
    suppressLineBreak = false;

    /* install class callbacks */
    RTFSetClassCallback(rtfText, TextClass);
    RTFSetClassCallback(rtfControl, ControlClass);

    /* install destination callbacks */
    RTFSetDestinationCallback(rtfObjWid, ReadObjWidth);
    RTFSetDestinationCallback(rtfColorTbl, WriteColors);
    RTFSetDestinationCallback(rtfObject, ReadObject);
    RTFSetDestinationCallback(rtfWord97Object, ReadWord97Object);
    RTFSetDestinationCallback(rtfPict, ReadPicture);
    RTFSetDestinationCallback(rtfFootnote, ReadFootnote);

    for (i=0; i<256; i++)
        UsedColor[i] = 0;
        
    WriteLaTeXHeader();
    return (1);
}


/* characters from the Symbol font get written to private areas 
   of unicode that are not well supported by latex.  This is 
   simple translation table. */
char *UnicodeSymbolFontToLatex[] = {
    " ",  /* 61472 or U+F020 */
    "!",
    "\\forall",
    "\\#",
    "\\exists",
    "\\%",
    "\\&",
    " ",
    "(",
    ")",
    "*",
    "+",
    ",",
    "-",
    ".",
    "/",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    ":",
    ";",
    "<",
    "=",
    ">",
    "?",
    "\\congruent",
    "A",
    "B",
    "X",
    "\\Delta",
    "E",
    "\\Phi",
    "\\Gamma",
    "H",
    "I",
    "\\vartheta",
    "K",
    "\\Lambda",
    "",
    "N",
    "O",
    "\\Pi",
    "\\Theta",
    "P",
    "\\Sigma",
    "T",
    "Y",
    "\\zeta",
    "\\Omega",
    "\\Xi",
    "\\Psi",
    "Z",
    "[",
    "\\therefore",
    "]",
    "\\bot",
    "\\_",
    "-",  /* over bar?? */
    "\\alpha",
    "\\beta",
    "\\chi",
    "\\delta",
    "\\epsilon",
    "\\phi",
    "\\gamma",
    "\\eta",
    "\\iota",
    "\\varphi",
    "\\kappa",
    "\\lambda",
    "\\mu",
    "\\nu",
    "o",
    "\\pi",
    "\\theta",
    "\\rho",
    "\\sigma",
    "\\tau",
    "\\nu",
    "\\varpi",
    "\\omega",
    "\\xi",
    "\\psi",
    "\\zeta",
    "\\lbrace",
    "|",
    "\\rbrace",
    "\\sim",
    "Y",
    "\\prime",
    "\\le",
    "/",
    "\\infty",
    "\\int",
    "\\clubsuit",
    "\\diamondsuit",
    "\\heartsuit",
    "\\spadesuit",
    "\\rightleftarrow",
    "\\leftarrow",
    "\\uparrow",
    "\\rightarrow",
    "\\downarrow",
    "^\\circ",
    "\\pm",
    "\\prime\\prime",
    "\\ge",
    "\\times",
    "\\propto",
    "\\partial",
    "\\blackcircle",
    "\\division",
    "\\ne",
    "\\equiv",
    "\\approx",
    "...",
    "|",
    "\\_",
    "", /*corner arrow*/
    "\\Aleph",
    0
};

/* greek characters translated directly to avoid the textgreek.sty package
   positions 913-969  (0x0391-0x03C9)*/
char *UnicodeGreekToLatex[] = {
    "A",
    "B",
    "\\Gamma",
    "\\Delta",
    "E",
    "Z",
    "H",
    "\\Theta",
    "I",
    "K",
    "\\Lambda",
    "M",
    "N",
    "\\Xi",
    "O",
    "\\Pi",
    "P",
    "", /* invalid character capital rho */
    "\\Sigma",
    "T",
    "Y",
    "\\Phi",
    "X",
    "\\Psi",
    "\\Omega",
    "\\\"I",
    "\\\"Y",
    "\\'\\alpha",
    "\\'\\epsilon",
    "\\'\\eta",
    "\\'\\iota",
    "\\\"u",
    "\\alpha",
    "\\beta",
    "\\gamma",
    "\\delta",
    "\\epsilon",
    "\\zeta",
    "\\eta",
    "\\theta",
    "\\iota",
    "\\kappa",
    "\\lambda",
    "\\mu",
    "\\nu",
    "\\xi",
    "o",
    "\\pi",
    "\\rho",
    "s",
    "\\sigma",
    "\\tau",
    "u",
    "\\phi",
    "\\chi",
    "\\psi",
    "\\omega",
    0
};
