/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#include "ESCParser.h"
#include <iomanip>

#include "zlib/zlib.h"


//////////////////////////////////////////////////////////////////////
// SVG driver

//NOTE: The most recent SVG standard is 1.2 tiny. Multipage support appears in 1.2 full.
// So, currently SVG does not have multipage support, and browsers can't interpret multipage SVGs.

void OutputDriverSvg::WriteBeginning(int pagestotal)
{
    m_output << "<?xml version=\"1.0\"?>" << std::endl;
    m_output << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.0\">" << std::endl;
}

void OutputDriverSvg::WriteEnding()
{
    m_output << "</svg>" << std::endl;
}

void OutputDriverSvg::WriteStrike(float x, float y, float r)
{
    float cx = x / 10.0f;
    float cy = y / 10.0f;
    float cr = r / 10.0f;
    m_output << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << cr << "\" />" << std::endl;
}


//////////////////////////////////////////////////////////////////////
// PostScript driver

void OutputDriverPostScript::WriteBeginning(int pagestotal)
{
    m_output << "%!PS-Adobe-2.0" << std::endl;
    m_output << "%%Creator: ESCParser" << std::endl;
    m_output << "%%Pages: " << pagestotal << std::endl;

    // PS procedure used to simplify WriteStrike output
    m_output << "/dotxyr { newpath 0 360 arc fill } def" << std::endl;
}

void OutputDriverPostScript::WriteEnding()
{
    m_output << "%%EOF" << std::endl;
}

void OutputDriverPostScript::WritePageBeginning(int pageno)
{
    m_output << "%%Page: " << pageno << " " << pageno << std::endl;
    m_output << "0 850 translate 1 -1 scale" << std::endl;
    m_output << "0 setgray" << std::endl;
}

void OutputDriverPostScript::WritePageEnding()
{
    m_output << "showpage" << std::endl;
}

void OutputDriverPostScript::WriteStrike(float x, float y, float r)
{
    float cx = x / 10.0f;
    float cy = y / 10.0f;
    float cr = r / 10.0f;

    char buffer[24];
    sprintf_s(buffer, sizeof(buffer), "%.2f %.2f %.1f", cx, cy, cr);
    m_output << buffer << " dotxyr" << std::endl;
}

//////////////////////////////////////////////////////////////////////
// PDF driver

// See below
void ascii85_encode_tuple(const unsigned char* src, char* dst);

const float PdfPageSizeX = 595.0f;  // A4 210mm / 25.4 * 72, rounded
const float PdfPageSizeY = 842.0f;  // A4 297mm / 25.4 * 72, rounded

void OutputDriverPdf::WriteBeginning(int pagestotal)
{
    xref.push_back(PdfXrefItem(0, 65535, 'f'));
    m_output << "%PDF-1.3" << std::endl;

    xref.push_back(PdfXrefItem(m_output.tellp(), 0, 'n'));
    m_output << "1 0 obj <<";
    m_output << "/Producer (ESCParser utility by Nikita Zimin)";
    m_output << ">>" << std::endl << "endobj" << std::endl;

    xref.push_back(PdfXrefItem(m_output.tellp(), 0, 'n'));
    m_output << "2 0 obj <</Type /Catalog /Pages 3 0 R>>" << std::endl;
    m_output << "endobj" << std::endl;

    xref.push_back(PdfXrefItem(m_output.tellp(), 0, 'n'));
    m_output << "3 0 obj <</Type /Pages /Kids [";
    for (int i = 0; i < pagestotal; i++)
    {
        if (i > 0)
            m_output << " ";
        m_output << i * 2 + 4 << " 0 R";  // Page objects: 4, 6, 8, etc.
    }
    m_output << "] /Count " << pagestotal << ">>" << std::endl;
    m_output << "endobj" << std::endl;
}

void OutputDriverPdf::WriteEnding()
{
    std::streamoff startxref = m_output.tellp();
    m_output << "xref" << std::endl;
    m_output << "0 " << xref.size() << std::endl;
    for (std::vector<PdfXrefItem>::iterator it = xref.begin(); it != xref.end(); ++it)
    {
        m_output << std::setw(10) << std::setfill('0') << (*it).offset << " ";
        m_output << std::setw(5) << (*it).size << " ";
        m_output << (*it).flag << std::endl;
    }

    m_output << "trailer" << std::endl;
    m_output << "<</Size " << xref.size() << " /Root 2 0 R /Info 1 0 R>>" << std::endl;
    m_output << "startxref" << std::endl;
    m_output << startxref << std::endl;
    m_output << "%%EOF" << std::endl;
}

void OutputDriverPdf::WritePageBeginning(int pageno)
{
    xref.push_back(PdfXrefItem(m_output.tellp(), 0, 'n'));
    int objnopage = pageno * 2 + 2;  // 4, 6, 8, etc.
    int objnostream = pageno * 2 + 3;  // 5, 7, 9, etc.
    m_output << objnopage << " 0 obj<</Type /Page /Parent 3 0 R ";
    m_output << "/MediaBox [0 0 595 842] ";  // Page bounds
    m_output << "/Contents " << objnostream << " 0 R ";
    m_output << "/Resources<< >> >>" << std::endl;  // Resources is required key
    m_output << "endobj" << std::endl;

    xref.push_back(PdfXrefItem(m_output.tellp(), 0, 'n'));
    m_output << objnostream << " 0 obj<<";

    strikesize = 0.0f;
    pagebuf.clear();
    pagebuf.append("1 J");  // Round cap
}

void OutputDriverPdf::WritePageEnding()
{
    // Preparing for inflate
    size_t outsize = pagebuf.length() + pagebuf.length() / 2 + 200;
    outsize = (outsize + 3) / 4 * 4;  // Make sure we have 4-byte aligned size
    Bytef* zbuffer = new Bytef[outsize];  memset(zbuffer, 0, outsize);
    z_stream zstrm;  memset(&zstrm, 0, sizeof(zstrm));
    zstrm.avail_in = pagebuf.length();
    zstrm.avail_out = outsize;
    zstrm.next_in = (Bytef*) pagebuf.c_str();
    zstrm.next_out = zbuffer;
    // Trying to inflate
    bool inflatedok = false;
    int rsti = deflateInit(&zstrm, Z_DEFAULT_COMPRESSION);
    if (rsti == Z_OK)
    {
        int rst2 = deflate(&zstrm, Z_FINISH);
        if (rst2 >= 0)
            inflatedok = true;
    }
    size_t inflatesize = zstrm.total_out;

    if (inflatedok)
    {
        std::string pagebufz;
        char buffer[6];  memset(buffer, 0, sizeof(buffer));
        for (size_t i = 0; i < inflatesize; i += 4)
        {
            unsigned char * bytes = zbuffer + i;
            ascii85_encode_tuple(bytes, buffer);
            pagebufz.append(buffer);
        }
        pagebufz.append("~>");

        m_output << "/Length " << pagebufz.length() << " /Filter [/ASCII85Decode /FlateDecode]>>stream" << std::endl;
        m_output << pagebufz.c_str() << std::endl;
        m_output << "endstream" << std::endl << "endobj" << std::endl;
    }
    else
    {
        m_output << "/Length " << pagebuf.length() << ">>stream" << std::endl;
        m_output << pagebuf.c_str() << std::endl;
        m_output << "endstream" << std::endl << "endobj" << std::endl;
    }

    deflateEnd(&zstrm);
    delete[] zbuffer;  zbuffer = 0;
}

void OutputDriverPdf::WriteStrike(float x, float y, float r)
{
    char buffer[80];

    if (strikesize != r / 5.0f)
    {
        strikesize = r / 5.0f;
        sprintf_s(buffer, sizeof(buffer), " %g w", strikesize);  // Line width
        pagebuf.append(buffer);
    }

    float cx = x / 10.0f;
    float cy = PdfPageSizeY - y / 10.0f;

    sprintf_s(buffer, sizeof(buffer), " %g %g m %g %g l s", cx, cy, cx, cy);
    pagebuf.append(buffer);
}

//////////////////////////////////////////////////////////////////////
// ASCII85 encoding for PDF

typedef unsigned int  uint32_t;
// make sure uint32_t is 32-bit
typedef char Z85_uint32_t_static_assert[(sizeof(uint32_t) * 8 == 32) * 2 - 1];

#define DIV85_MAGIC 3233857729ULL
// make sure magic constant is 64-bit
typedef char Z85_div85_magic_static_assert[(sizeof(DIV85_MAGIC) * 8 == 64) * 2 - 1];

#define DIV85(number) ((uint32_t)((DIV85_MAGIC * number) >> 32) >> 6)

static const char* base85 =
    "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstu";

void ascii85_encode_tuple(const unsigned char* src, char* dst)
{
    uint32_t value;
    uint32_t value2;

    // unpack big-endian frame
    value = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

    if (value == 0)  // Special case for zero
    {
        dst[0] = 'z';
        dst[2] = dst[3] = dst[4] = dst[5] = 0;
    }
    else
    {
        value2 = DIV85(value); dst[4] = base85[value - value2 * 85]; value = value2;
        value2 = DIV85(value); dst[3] = base85[value - value2 * 85]; value = value2;
        value2 = DIV85(value); dst[2] = base85[value - value2 * 85]; value = value2;
        value2 = DIV85(value); dst[1] = base85[value - value2 * 85];
        dst[0] = base85[value2];
    }
}

//////////////////////////////////////////////////////////////////////
