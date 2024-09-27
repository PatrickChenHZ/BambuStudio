#include "StepMeshDialog.hpp"

#include <thread>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/TextInput.hpp"
#include <chrono>

using namespace Slic3r;
using namespace Slic3r::GUI;

static int _scale(const int val) { return val * Slic3r::GUI::wxGetApp().em_unit() / 10; }
static int _ITEM_WIDTH() { return _scale(30); }
#define MIN_DIALOG_WIDTH        FromDIP(400)
#define SLIDER_WIDTH            FromDIP(150)
#define TEXT_CTRL_WIDTH         FromDIP(50)
#define BUTTON_SIZE             wxSize(FromDIP(58), FromDIP(24))
#define BUTTON_BORDER           FromDIP(int(400 - 58 * 2) / 8)
#define SLIDER_SCALE(val)       ((val) / 0.001)
#define SLIDER_UNSCALE(val)     ((val) * 0.001)
#define SLIDER_SCALE_10(val)    ((val) / 0.01)
#define SLIDER_UNSCALE_10(val)  ((val) * 0.01)
#define LEFT_RIGHT_PADING       FromDIP(20)

wxDEFINE_EVENT(wxEVT_THREAD_DONE, wxCommandEvent);

void StepMeshDialog::on_dpi_changed(const wxRect& suggested_rect) {
};

bool StepMeshDialog:: validate_number_range(const wxString& value, double min, double max) {
    double num = 0.0;
    if (value.IsEmpty()) {
        return false;
    }
    try {
        if (!value.ToDouble(&num)) {
            return false;
        }
    } catch (...) {
        return false;
    }

    return (num >= min && num <= max);
}

StepMeshDialog::StepMeshDialog(wxWindow* parent, Slic3r::Step& file)
    : DPIDialog(parent ? parent : static_cast<wxWindow *>(wxGetApp().mainframe),
                wxID_ANY,
                _(L("Step file import parameters")),
                wxDefaultPosition,
                wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE /* | wxRESIZE_BORDER*/), m_file(file)
{
    Bind(wxEVT_THREAD_DONE, &StepMeshDialog::on_task_done, this);

    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico")
                             % Slic3r::resources_dir()).str();
    SetIcon(wxIcon(Slic3r::encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    SetBackgroundColour(*wxWHITE);

    wxBoxSizer* bSizer = new wxBoxSizer(wxVERTICAL);
    bSizer->SetMinSize(wxSize(MIN_DIALOG_WIDTH, -1));

    wxBoxSizer* linear_sizer = new wxBoxSizer(wxHORIZONTAL);
    //linear_sizer->SetMinSize(wxSize(MIN_DIALOG_WIDTH, -1));
    wxStaticText* linear_title = new wxStaticText(this,
                                                  wxID_ANY,
                                                  _L("Linear Deflection:"));
    linear_sizer->Add(linear_title, 0, wxALIGN_LEFT);
    linear_sizer->AddStretchSpacer(1);
    wxSlider* linear_slider = new wxSlider(this, wxID_ANY,
                                           SLIDER_SCALE(get_linear_defletion()),
                                           1, 100, wxDefaultPosition,
                                           wxSize(SLIDER_WIDTH, -1),
                                           wxSL_HORIZONTAL);
    linear_sizer->Add(linear_slider, 0, wxALIGN_RIGHT | wxLEFT, FromDIP(5));

    auto linear_input = new ::TextInput(this, m_linear_last, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(TEXT_CTRL_WIDTH, -1));
    linear_input->GetTextCtrl()->SetFont(Label::Body_12);
    linear_input->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    linear_sizer->Add(linear_input, 0, wxALIGN_RIGHT | wxLEFT, FromDIP(5));
    linear_input->Bind(wxEVT_KILL_FOCUS, ([this, linear_input](wxFocusEvent& e) {
        wxString value = linear_input->GetTextCtrl()->GetValue();
        if (validate_number_range(value, 0.001, 0.1)) {
            m_linear_last = value;
            update_mesh_number_text();
        } else {
            MessageDialog msg_dlg(nullptr, _L("Please input a valid value (0.001 < angle deflection < 0.1)"), wxEmptyString, wxICON_WARNING | wxOK);
            msg_dlg.ShowModal();
            linear_input->GetTextCtrl()->SetValue(m_linear_last);
        }
        e.Skip();
    }));
    // textctrl bind slider
    linear_input->Bind(wxEVT_TEXT, ([this, linear_slider, linear_input](wxCommandEvent& e) {
        double slider_value_long;
        int slider_value;
        wxString value = linear_input->GetTextCtrl()->GetValue();
        if (value.ToDouble(&slider_value_long)) {
            slider_value = SLIDER_SCALE(slider_value_long);
            if (slider_value >= linear_slider->GetMin() && slider_value <= linear_slider->GetMax()) {
                linear_slider->SetValue(slider_value);
            }
        }
    }));
    linear_slider->Bind(wxEVT_SLIDER, ([this, linear_slider, linear_input](wxCommandEvent& e) {
        double slider_value = SLIDER_UNSCALE(linear_slider->GetValue());
        linear_input->GetTextCtrl()->SetValue(wxString::Format("%.3f", slider_value));
        m_linear_last = wxString::Format("%.3f", slider_value);
    }));
    linear_slider->Bind(wxEVT_LEFT_UP, ([this](wxMouseEvent& e) {
        update_mesh_number_text();
        e.Skip();
    }));

    bSizer->Add(linear_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, LEFT_RIGHT_PADING);

    wxBoxSizer* angle_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* angle_title = new wxStaticText(this,
                                                  wxID_ANY,
                                                  _L("Angle Deflection:"));
    angle_sizer->Add(angle_title, 0, wxALIGN_LEFT);
    angle_sizer->AddStretchSpacer(1);
    wxSlider* angle_slider = new wxSlider(this, wxID_ANY,
                                           SLIDER_SCALE_10(get_angle_defletion()),
                                           1, 100, wxDefaultPosition,
                                           wxSize(SLIDER_WIDTH, -1),
                                           wxSL_HORIZONTAL);
    angle_sizer->Add(angle_slider, 0, wxALIGN_RIGHT | wxLEFT, FromDIP(5));

    auto angle_input = new::TextInput(this, m_angle_last, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(TEXT_CTRL_WIDTH, -1));
    angle_input->GetTextCtrl()->SetFont(Label::Body_12);
    angle_input->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    angle_sizer->Add(angle_input, 0, wxALIGN_RIGHT | wxLEFT, FromDIP(5));
    angle_input->Bind(wxEVT_KILL_FOCUS, ([this, angle_input](wxFocusEvent& e) {
        wxString value = angle_input->GetTextCtrl()->GetValue();
        if (validate_number_range(value, 0.01, 1)) {
            m_angle_last = value;
            update_mesh_number_text();
        } else {
            MessageDialog msg_dlg(nullptr, _L("Please input a valid value (0.01 < angle deflection < 1.0)"), wxEmptyString, wxICON_WARNING | wxOK);
            msg_dlg.ShowModal();
            angle_input->GetTextCtrl()->SetValue(m_angle_last);
        }
        e.Skip();
    }));
    // textctrl bind slider
    angle_input->Bind(wxEVT_TEXT, ([this, angle_slider, angle_input](wxCommandEvent& e) {
        double slider_value_long;
        int slider_value;
        wxString value = angle_input->GetTextCtrl()->GetValue();
        if (value.ToDouble(&slider_value_long)) {
            slider_value = SLIDER_SCALE_10(slider_value_long);
            if (slider_value >= angle_slider->GetMin() && slider_value <= angle_slider->GetMax()) {
                angle_slider->SetValue(slider_value);
            }
        }
    }));

    angle_slider->Bind(wxEVT_SLIDER, ([this, angle_slider, angle_input](wxCommandEvent& e) {
        double slider_value = SLIDER_UNSCALE_10(angle_slider->GetValue());
        angle_input->GetTextCtrl()->SetValue(wxString::Format("%.2f", slider_value));
        m_angle_last = wxString::Format("%.2f", slider_value);
    }));
    angle_slider->Bind(wxEVT_LEFT_UP, ([this](wxMouseEvent& e) {
        update_mesh_number_text();
        e.Skip();
    }));

    bSizer->Add(angle_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, LEFT_RIGHT_PADING);

    wxBoxSizer* mesh_face_number_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* mesh_face_number_title = new wxStaticText(this, wxID_ANY, _L("Number of triangular facets: "));
    mesh_face_number_text = new wxStaticText(this, wxID_ANY, _L("0"));
    mesh_face_number_text->SetMinSize(wxSize(FromDIP(150), -1));
    mesh_face_number_sizer->Add(mesh_face_number_title, 0, wxALIGN_LEFT);
    mesh_face_number_sizer->Add(mesh_face_number_text, 0, wxALIGN_LEFT);
    bSizer->Add(mesh_face_number_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, LEFT_RIGHT_PADING);

    wxBoxSizer* bSizer_button = new wxBoxSizer(wxHORIZONTAL);
    bSizer_button->SetMinSize(wxSize(FromDIP(100), -1));

    StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed), std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
                            std::pair<wxColour, int>(AMS_CONTROL_BRAND_COLOUR, StateColor::Normal));
    m_button_ok = new Button(this, _L("OK"));
    m_button_ok->SetBackgroundColor(btn_bg_green);
    m_button_ok->SetBorderColor(*wxWHITE);
    m_button_ok->SetTextColor(wxColour(0xFFFFFE));
    m_button_ok->SetFont(Label::Body_12);
    m_button_ok->SetSize(BUTTON_SIZE);
    m_button_ok->SetMinSize(BUTTON_SIZE);
    m_button_ok->SetCornerRadius(FromDIP(12));
    bSizer_button->Add(m_button_ok, 0, wxALIGN_RIGHT, BUTTON_BORDER);

    m_button_ok->Bind(wxEVT_LEFT_DOWN, [this, angle_input, linear_input](wxMouseEvent& e) {
        stop_task();
        if (validate_number_range(angle_input->GetTextCtrl()->GetValue(), 0.01, 1) &&
            validate_number_range(linear_input->GetTextCtrl()->GetValue(), 0.001, 0.1)) {
            EndModal(wxID_OK);
        }
        SetFocusIgnoringChildren();
    });

    StateColor btn_bg_white(std::pair<wxColour, int>(wxColour(206, 206, 206), StateColor::Pressed), std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
                            std::pair<wxColour, int>(*wxWHITE, StateColor::Normal));

    m_button_cancel = new Button(this, _L("Cancel"));
    m_button_cancel->SetBackgroundColor(btn_bg_white);
    m_button_cancel->SetBorderColor(wxColour(38, 46, 48));
    m_button_cancel->SetFont(Label::Body_12);
    m_button_cancel->SetSize(BUTTON_SIZE);
    m_button_cancel->SetMinSize(BUTTON_SIZE);
    m_button_cancel->SetCornerRadius(FromDIP(12));
    bSizer_button->Add(m_button_cancel, 0, wxALIGN_RIGHT | wxLEFT, BUTTON_BORDER);

    m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        stop_task();
        EndModal(wxID_CANCEL);
    });

    bSizer->Add(bSizer_button, 0, wxALIGN_RIGHT | wxRIGHT| wxBOTTOM, LEFT_RIGHT_PADING);

    this->SetSizer(bSizer);
    update_mesh_number_text();
    this->Layout();
    bSizer->Fit(this);

    this->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        SetFocusIgnoringChildren();
    });
    mesh_face_number_text->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        SetFocusIgnoringChildren();
    });

    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
        stop_task();
        EndModal(wxID_CANCEL);
    });

    wxGetApp().UpdateDlgDarkUI(this);
}

void StepMeshDialog::on_task_done(wxCommandEvent& event)
{
    wxString text = event.GetString();
    mesh_face_number_text->SetLabel(text);
    if (task.valid()) {
        task.get();
    }
}

void StepMeshDialog::stop_task()
{
    if (task.valid()) {
        m_file.m_stop_mesh.store(true);
        unsigned int test = task.get();
        m_file.m_stop_mesh.store(false);
        std::cout << test << std::endl;
    }

}

void StepMeshDialog::update_mesh_number_text()
{
    if (m_last_linear == get_linear_defletion() && m_last_angle == get_angle_defletion())
        return;
    wxString newText = wxString::Format(_L("Calculating, please wait..."));
    mesh_face_number_text->SetLabel(newText);

    stop_task();
    task = std::async(std::launch::async, [&] {
        unsigned int number = m_file.get_triangle_num(get_linear_defletion(), get_angle_defletion());
        if (number != 0) {
            wxString number_text = wxString::Format("%d", number);
            wxCommandEvent event(wxEVT_THREAD_DONE);
            event.SetString(number_text);
            wxPostEvent(this, event);
        }
        return number;
    });
}