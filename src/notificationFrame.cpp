#include <iostream>
#include <stdexcept>
#include <regex>
#include <wx/event.h>
#include <wx/dcclient.h>
#include <wx/settings.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/display.h>
#include <wx/app.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <wx/intl.h>
#include <wx/protocol/http.h>
#include <wx/sstream.h>
#include <json/value.h>
#include <json/reader.h>
#include <curl/curl.h>
#include <wx/stdpaths.h>

#include "notificationFrame.h"
#include "controller.h"
#include "version.h"
#include "chanstatus.h"
#include "iconmacro.h"

struct MemoryStruct {
	  char *memory;
	    size_t size;
};

const long notificationFrame::ID_HTMLWINDOW1 = wxNewId();
const long notificationFrame::ID_BUTTON1 = wxNewId();

void ExecCommand(wxString &cmd, wxArrayString &output)
{
    wxProcess p;
    p.Redirect();
    wxExecute(cmd, wxEXEC_SYNC, &p);

    wxInputStream *i = p.GetInputStream();
    if(i)
    {
        wxTextInputStream t(*i, " \t", wxConvUTF8);

        while(!i->Eof())
        {
            output.Add(t.ReadLine());
        }
    }
    if (p.IsErrorAvailable())
    {
        wxInputStream *e = p.GetErrorStream();
        wxString error_string;
        while (!e->Eof())
        {
            wxTextInputStream t(*e, " \t", wxConvUTF8);
            error_string = t.ReadLine();
            if (!error_string.empty())
            {
                std::cerr << "error: '" << error_string << "'" << std::endl;
            }
        }
    }
}

void FindAndReplace(std::string &tmpl, std::string varname, std::string value)
{
	size_t start_pos = 0;
	size_t found_pos;
	while ((found_pos = tmpl.find(varname, start_pos)) != std::string::npos)
	{
		start_pos = found_pos + 1;
		tmpl.replace(found_pos, varname.length(), value);
	}
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = (char *) realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

notificationFrame::notificationFrame(wxWindow* parent, ChannelStatusPool *pool)
    : ChannelStatusPooler(pool)
{
	edescr = "notify frame";
	wxBoxSizer* BoxSizerButtons;

	Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP|wxFRAME_NO_TASKBAR|wxFRAME_TOOL_WINDOW|wxNO_BORDER);
	SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	BoxSizer1 = new wxBoxSizer(wxVERTICAL);
	HtmlWindow1 = new wxHtmlWindow(this, ID_HTMLWINDOW1, wxDefaultPosition, wxSize(400,30), wxHW_SCROLLBAR_NEVER|wxHW_NO_SELECTION);
	HtmlWindow1->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
	HtmlWindow1->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	BoxSizer1->Add(HtmlWindow1, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 1);
	BoxSizerButtons = new wxBoxSizer(wxHORIZONTAL);
	Button1 = new wxButton(this, ID_BUTTON1, _("No"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    wxBitmap hangupIcon;
    hangupIcon.CopyFromIcon(ACTI_ICON_SIZED("hangup", 24));
    Button1->SetBitmap(hangupIcon);
	BoxSizerButtons->Add(Button1, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 1);
	BoxSizer1->Add(BoxSizerButtons, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
	SetSizer(BoxSizer1);
	BoxSizer1->Fit(this);
	BoxSizer1->SetSizeHints(this);
	m_hidetimer = new wxTimer(this);
#ifdef DEBUG
    wxButton *hidebutton = new wxButton(HtmlWindow1, wxID_ANY, wxT("×"), wxPoint(380,0), wxSize(20,20), wxBORDER_NONE);
	hidebutton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &notificationFrame::OnHideButtonClick, this);
#endif

	Connect(ID_BUTTON1,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&notificationFrame::OnButton1Click);
	Connect(wxEVT_PAINT,(wxObjectEventFunction)&notificationFrame::OnPaint);
	Bind(wxEVT_HTML_LINK_CLICKED, &notificationFrame::OnLinkClicked, this);
	Bind(wxEVT_TIMER, &notificationFrame::OnHideTimer, this);
    m_lookup_enabled = false;
}

notificationFrame::~notificationFrame()
{
    delete m_hidetimer;
}

void notificationFrame::SetLookupCmd(std::string cmd) {
	m_lookup_cmd = cmd;
    m_lookup_enabled |= (!cmd.empty());
}

void notificationFrame::SetLookupUrl(std::string url) {
	m_lookup_url = url;
    m_lookup_enabled |= (!url.empty());
}


void notificationFrame::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	wxColor fg,bg,middle;
	fg = wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT);
	bg = wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK);
	middle = wxColour((fg.Red()+bg.Red()*2)/3, (fg.Green()+bg.Green()*2)/3, (fg.Blue()+bg.Blue()*2)/3);
	dc.SetPen(middle);
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	wxSize sz(GetSize());
	dc.DrawRectangle(0, 0, sz.x, sz.y);
	event.Skip();
}

void notificationFrame::OnButton1Click(wxCommandEvent& event)
{
	m_controller->HangupChannel(m_current_channel);
}

#ifdef DEBUG
void notificationFrame::OnHideButtonClick(wxCommandEvent& event)
{
	Hide();
}
#endif

void notificationFrame::OnHideTimer(wxTimerEvent &event)
{
	Hide();
}

void notificationFrame::OnLinkClicked(wxHtmlLinkEvent& event)
{
	wxLaunchDefaultBrowser(event.GetLinkInfo().GetHref());
}

void notificationFrame::SetHtml(const wxString &s)
{
	wxString bgcolor = wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK).GetAsString();
	HtmlWindow1->SetPage("<body bgcolor='"+bgcolor+"'>"+s+"</body>");
	int height = HtmlWindow1->GetInternalRepresentation()->GetHeight();
	int width = HtmlWindow1->GetInternalRepresentation()->GetWidth();
	BoxSizer1->SetItemMinSize(HtmlWindow1, width, height);
	HtmlWindow1->Fit();
	Fit();
	wxDisplay display;
    	wxRect rect = display.GetClientArea();
    	wxSize size = GetSize();
    	Move(rect.GetBottomRight()-size-wxSize(0,4));
 
}

void notificationFrame::UpdateSize()
{
}

void notificationFrame::OnUp(const AmiMessage &message)
{
	long timer = m_controller->CfgInt("gui/pickup_notify_hide_timeout");
	if (!timer){
	       Hide();
	       return;
	}
	else if (timer != -1)
		m_hidetimer->StartOnce(timer);
}

void notificationFrame::OnDialIn(const AmiMessage &message)
{
 	std::string callerid = message["CallerIDNum"];
	wxString html = "";
	html << wxT("<h5><font face='pt sans,tahoma,sans'>☎ ") + message["CallerIDNum"];
	if (!message["CallerIDName"].empty() && message["CallerIDName"] != "<unknown>")
    {
		html << " (" << message["CallerIDName"] << ")";
    }
	html << "</font></h5>";
	m_current_channel = message["Destination"];

    std::list<Channel *> peers = m_channelstatuspool->getBridgedChannelsOf(message["Channel"]);
    if (peers.size() == 1)
    {
        Channel *peer = *peers.begin();
        std::string transferred_calleridnum = peer->m_bridgedTo->m_callerIDNum;
        std::string transferred_calleridname = "";
        if (peer->m_bridgedTo->m_callerIDName != peer->m_bridgedTo->m_callerIDNum)
            transferred_calleridname = peer->m_bridgedTo->m_callerIDName;
        //StatusText->AppendText(m["CallerIDNum"] + " ("+ m["CallerIDName"] +") is transferring: " + transferred_calleridnum + " (" + transferred_calleridname + ")\n\n");
        html << "<h5>&#8627; " << transferred_calleridnum << " (" << transferred_calleridname << ")</h5>";
        callerid = transferred_calleridnum;
    }


	bool number_matches = false;
	if (m_lookup_enabled && callerid != m_controller->GetMyExten())
	{
		std::string regex = m_controller->Cfg("lookup/number_match_regex");
		if (!regex.empty())
		{
			try {
				number_matches = std::regex_match(callerid, std::regex(regex));
				std::cerr << "Regex: '" << regex << "' matches: " << number_matches << std::endl;
			} catch (std::regex_error)
			{
				std::cerr << "Regex only implemented in stdc++ 4.9 or later." << std::endl
					<< "Try to use number_min_length option in astercti.ini" << std::endl;
			}
		}
		else
		{
			int number_length = m_controller->CfgInt("lookup/number_min_length");
			if (number_length && callerid.length() >= number_length)
				number_matches = true;
		}

	}
	if (!callerid.empty() && callerid != "<unknown>" && m_lookup_enabled  && number_matches)
	{
		SetHtml(html + "<br><img src='/usr/share/astercti/wait.gif'>");
		ShowWithoutActivating();
		wxString out = Lookup(callerid);
		SetHtml(html+"<br />" + out);
		//std::cout << out << std::endl;
	}
	else if (!html.empty())
	{
		SetHtml(html);
	}
	ShowWithoutActivating();
}

void notificationFrame::OnHangup(const AmiMessage &message)
{
	Hide();
}

void notificationFrame::OnDial(const AmiMessage &message)
{
	m_current_channel = message["Channel"];
}

void notificationFrame::OnOriginate(const AmiMessage &message)
{
	m_current_channel = message["Channel"];
	wxString html = _("Pickup the handset to dial") + " <b>" + message["ConnectedLineName"] + "</b>";
	SetHtml(html);
	ShowWithoutActivating();
}

wxString notificationFrame::Lookup(std::string callerid)
{
	wxString out;
	wxArrayString output;
	wxString cmd;

    if (!m_lookup_url.empty())
    {
        CURL *curl;
        CURLcode res;
        struct MemoryStruct chunk;
        chunk.memory = NULL;
        chunk.size = 0;
        curl = curl_easy_init();
        if (curl)
        {
            char *url = new char[ m_lookup_url.length() + callerid.length() + 1 ];
            sprintf(url, m_lookup_url.c_str(), callerid.c_str());
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "astercti/" VERSION);
            if (m_controller->CfgBool("lookup/curl_insecure", false))
            {
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
            }
#ifdef __WXMSW__
            else
            {
                curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
            }
#endif
            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                wxLogError("curl failed: %s", curl_easy_strerror(res));
            }
            else
            {
                out = wxString::FromUTF8((const char *)chunk.memory);
            }
            curl_easy_cleanup(curl);
            free(chunk.memory);
            delete[] url;
        }
    }
    else if (!m_lookup_cmd.empty())
    {
        cmd.Printf(wxString(m_lookup_cmd), callerid);
        ExecCommand(cmd, output);
        for (auto iter : output)
        {
            out += iter;
        }
    }
    else
    {
        wxLogWarning(_("Lookup URL and Lookup command are both unconfigured.\nLookup disabled."));
    }

	Json::Value root;
	Json::Reader reader;
	std::string outstring = std::string(out.mb_str());
	if (!reader.parse(outstring, root, false))
	{
		std::cerr << "Failed to parse JSON: " << std::endl
		       << reader.getFormattedErrorMessages() <<std::endl
		       << "----" << std::endl
		       << out << std::endl;
	}
	const Json::Value clients = root["clients"];
	wxString html;
	std::string url_template = m_controller->Cfg("templates/client_url");
	for ( int i = 0; i < clients.size() && i < 3; ++i )
	{
		std::string url = url_template;
		Json::Value::Members client_attrs = clients[i].getMemberNames();
		for (unsigned int attr_index = 0; attr_index < client_attrs.size(); ++attr_index)
		{
			std::string variable_name = client_attrs[attr_index];
			std::string template_name = "${" + variable_name + "}";
			if (clients[i][variable_name].isConvertibleTo(Json::ValueType::stringValue))
				FindAndReplace(url, template_name, clients[i][variable_name].asString());
		}
		html += "<a href='"+ url +"'>" +clients[i]["name"].asString() + "</a><br />";
		if (clients[i]["services"].size())
		{
			html += "<ul>";
			for (int srv = 0; srv < clients[i]["services"].size(); ++srv)
			{
				html += "<li>" + clients[i]["services"][srv].asString() + "</li>";
			}
			html += "</ul>";
		}
		html += "<br />";
		if (i+1 < clients.size())
			html += "<br />";
	}
	if (clients.size() > 3)
	{
		html += "...<br/>";
	}
	if (clients.size() > 1)
	{
		wxString url_tpl = m_controller->Cfg("lookup/search_url");
		wxString url;
		url.Printf(url_tpl, callerid);
		html << "<hr size=1 noshade /><a href='" << url << "'>" << _("See all found") << " (" << clients.size() << ")</a>";
	}
	else if (clients.size() == 0)
	{
		html << _("Unknown number.");
	}

	return html;
}
