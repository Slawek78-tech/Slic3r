#include "GUI_ObjectManipulation.hpp"
#include "GUI_ObjectList.hpp"

#include "OptionsGroup.hpp"
#include "wxExtensions.hpp"
#include "PresetBundle.hpp"
#include "Model.hpp"
#include "Geometry.hpp"

#include <boost/algorithm/string.hpp>

namespace Slic3r
{
namespace GUI
{

ObjectManipulation::ObjectManipulation(wxWindow* parent):
    OG_Settings(parent, true)
{
    m_og->set_name(_(L("Object Manipulation")));
    m_og->label_width = 100;
    m_og->set_grid_vgap(5);

    m_og->m_on_change = [this](t_config_option_key opt_key, boost::any value) {
        if (opt_key == "scale_unit") {
            const wxString& selection = boost::any_cast<wxString>(value);
            std::vector<std::string> axes{ "x", "y", "z" };
            for (auto axis : axes) {
                std::string key = "scale_" + axis;
                m_og->set_side_text(key, selection);
            }

            m_is_percent_scale = selection == _("%");
            update_scale_values();
        }
    };

    ConfigOptionDef def;

    // Objects(sub-objects) name
    def.label = L("Name");
    // 	def.type = coString;
    def.gui_type = "legend";
    def.tooltip = L("Object name");
    def.full_width = true;
    def.default_value = new ConfigOptionString{ " " };
    m_og->append_single_option_line(Option(def, "object_name"));

    // Legend for object modification
    auto line = Line{ "", "" };
    def.label = "";
    def.type = coString;
    def.width = 55;

    std::vector<std::string> axes{ "x", "y", "z" };
    for (const auto axis : axes) {
        const auto label = boost::algorithm::to_upper_copy(axis);
        def.default_value = new ConfigOptionString{ "   " + label };
        Option option = Option(def, axis + "_axis_legend");
        line.append_option(option);
    }
    m_og->append_line(line);


    auto add_og_to_object_settings = [](const std::string& option_name, const std::string& sidetext)
    {
        int def_value = 0;
        Line line = { _(option_name), "" };
        if (option_name == "Scale") {
            line.near_label_widget = [](wxWindow* parent) {
                auto btn = new PrusaLockButton(parent, wxID_ANY);
                btn->Bind(wxEVT_BUTTON, [btn](wxCommandEvent &event) {
                    event.Skip();
                    wxTheApp->CallAfter([btn]() {
                        wxGetApp().obj_manipul()->set_uniform_scaling(btn->IsLocked());
                    });
                });
                return btn;
            };
        }

        ConfigOptionDef def;
        def.type = coInt;
        def.default_value = new ConfigOptionInt(def_value);
        def.width = 55;

        if (option_name == "Rotation")
            def.min = -360;

        const std::string lower_name = boost::algorithm::to_lower_copy(option_name);

        std::vector<std::string> axes{ "x", "y", "z" };
        for (auto axis : axes) {
            if (axis == "z" && option_name != "Scale")
                def.sidetext = sidetext;
            Option option = Option(def, lower_name + "_" + axis);
            option.opt.full_width = true;
            line.append_option(option);
        }

        if (option_name == "Scale")
        {
            def.width = 45;
            def.type = coStrings;
            def.gui_type = "select_open";
            def.enum_labels.push_back(L("%"));
            def.enum_labels.push_back(L("mm"));
            def.default_value = new ConfigOptionStrings{ "mm" };

            const Option option = Option(def, lower_name + "_unit");
            line.append_option(option);
        }

        return line;
    };


    // Settings table
    m_og->append_line(add_og_to_object_settings(L("Position"), L("mm")));
    m_og->append_line(add_og_to_object_settings(L("Rotation"), "°"));
    m_og->append_line(add_og_to_object_settings(L("Scale"), "mm"));


    def.label = L("Place on bed");
    def.type = coBool;
    def.tooltip = L("Automatic placing of models on printing bed in Y axis");
    def.gui_type = "";
    def.sidetext = "";
    def.default_value = new ConfigOptionBool{ false };
    m_og->append_single_option_line(Option(def, "place_on_bed"));
}

void ObjectManipulation::Show(const bool show)
{
    if (show == IsShown())
        return;

    m_og->Show(show);

    if (show && wxGetApp().get_view_mode() != ConfigMenuModeSimple) {
        m_og->get_grid_sizer()->Show(size_t(0), false);
        m_og->get_grid_sizer()->Show(size_t(1), false);
    }
}

bool ObjectManipulation::IsShown()
{
    return m_og->get_grid_sizer()->IsShown(2);
}

void ObjectManipulation::UpdateAndShow(const bool show)
{
    if (show)
        update_settings_value(_3DScene::get_canvas(wxGetApp().canvas3D())->get_selection());

    OG_Settings::UpdateAndShow(show);
}

int ObjectManipulation::ol_selection()
{
    return wxGetApp().obj_list()->get_selected_obj_idx();
}

void ObjectManipulation::update_settings_value(const GLCanvas3D::Selection& selection)
{
#if ENABLE_MODELVOLUME_TRANSFORM
    if (selection.is_single_full_instance())
#else
    if (selection.is_single_full_object())
    {
        auto obj_idx = selection.get_object_idx();
        if (obj_idx >=0 && !wxGetApp().model_objects()->empty() && (*wxGetApp().model_objects())[obj_idx]->instances.size() == 1)
        {
            // all volumes in the selection belongs to the same instance, any of them contains the needed data, so we take the first
            const GLVolume* volume = selection.get_volume(*selection.get_volume_idxs().begin());
            update_position_value(volume->get_offset());
            update_rotation_value(volume->get_rotation());
            update_scale_value(volume->get_scaling_factor());
            m_og->enable();
        }
        else
            reset_settings_value();
    }
    else if (selection.is_single_full_instance())
#endif // ENABLE_MODELVOLUME_TRANSFORM
    {
        // all volumes in the selection belongs to the same instance, any of them contains the needed data, so we take the first
        const GLVolume* volume = selection.get_volume(*selection.get_volume_idxs().begin());
#if ENABLE_MODELVOLUME_TRANSFORM
        update_position_value(volume->get_instance_offset());
        update_rotation_value(volume->get_instance_rotation());
        update_scale_value(volume->get_instance_scaling_factor());
#else
        update_position_value(volume->get_offset());
        update_rotation_value(volume->get_rotation());
        update_scale_value(volume->get_scaling_factor());
#endif // ENABLE_MODELVOLUME_TRANSFORM
        m_og->enable();
    }
    else if (selection.is_wipe_tower())
    {
        // the selection contains a single volume
        const GLVolume* volume = selection.get_volume(*selection.get_volume_idxs().begin());
#if ENABLE_MODELVOLUME_TRANSFORM
        update_position_value(volume->get_volume_offset());
        update_rotation_value(volume->get_volume_rotation());
        update_scale_value(volume->get_volume_scaling_factor());
#else
        update_position_value(volume->get_offset());
        update_rotation_value(volume->get_rotation());
        update_scale_value(volume->get_scaling_factor());
#endif // ENABLE_MODELVOLUME_TRANSFORM
        m_og->enable();
    }
    else if (selection.is_modifier())
    {
        // the selection contains a single volume
        const GLVolume* volume = selection.get_volume(*selection.get_volume_idxs().begin());
#if ENABLE_MODELVOLUME_TRANSFORM
        update_position_value(volume->get_volume_offset());
        update_rotation_value(volume->get_volume_rotation());
        update_scale_value(volume->get_volume_scaling_factor());
#else
        update_position_value(volume->get_offset());
        update_rotation_value(volume->get_rotation());
        update_scale_value(volume->get_scaling_factor());
#endif // ENABLE_MODELVOLUME_TRANSFORM
        m_og->enable();
    }
    else
        reset_settings_value();
}

void ObjectManipulation::reset_settings_value()
{
    reset_position_value();
    reset_rotation_value();
    reset_scale_value();
    m_og->disable();
}

void ObjectManipulation::reset_position_value()
{
    m_og->set_value("position_x", 0);
    m_og->set_value("position_y", 0);
    m_og->set_value("position_z", 0);
}

void ObjectManipulation::reset_rotation_value()
{
    m_og->set_value("rotation_x", 0);
    m_og->set_value("rotation_y", 0);
    m_og->set_value("rotation_z", 0);
}

void ObjectManipulation::reset_scale_value()
{
    m_is_percent_scale = true;
    m_og->set_value("scale_unit", _("%"));
    m_og->set_value("scale_x", 100);
    m_og->set_value("scale_y", 100);
    m_og->set_value("scale_z", 100);
}

void ObjectManipulation::update_values()
{
    int selection = ol_selection();
    if (selection < 0 || wxGetApp().mainframe->m_plater->model().objects.size() <= selection) {
        m_og->set_value("position_x", 0);
        m_og->set_value("position_y", 0);
        m_og->set_value("position_z", 0);
        m_og->set_value("scale_x", 0);
        m_og->set_value("scale_y", 0);
        m_og->set_value("scale_z", 0);
        m_og->set_value("rotation_x", 0);
        m_og->set_value("rotation_y", 0);
        m_og->set_value("rotation_z", 0);
        m_og->disable();
        return;
    }
    m_is_percent_scale = boost::any_cast<wxString>(m_og->get_value("scale_unit")) == _("%");

    update_position_values();
    update_scale_values();
    update_rotation_values();
    m_og->enable();
}

void ObjectManipulation::update_scale_values()
{
    int selection = ol_selection();
    ModelObjectPtrs& objects = wxGetApp().mainframe->m_plater->model().objects;

    auto instance = objects[selection]->instances.front();
    auto size = objects[selection]->instance_bounding_box(0).size();

    if (m_is_percent_scale) {
        m_og->set_value("scale_x", int(instance->get_scaling_factor(X) * 100));
        m_og->set_value("scale_y", int(instance->get_scaling_factor(Y) * 100));
        m_og->set_value("scale_z", int(instance->get_scaling_factor(Z) * 100));
    }
    else {
        m_og->set_value("scale_x", int(instance->get_scaling_factor(X) * size(0) + 0.5));
        m_og->set_value("scale_y", int(instance->get_scaling_factor(Y) * size(1) + 0.5));
        m_og->set_value("scale_z", int(instance->get_scaling_factor(Z) * size(2) + 0.5));
    }
}

void ObjectManipulation::update_position_values()
{
    auto instance = wxGetApp().mainframe->m_plater->model().objects[ol_selection()]->instances.front();

    m_og->set_value("position_x", int(instance->get_offset(X)));
    m_og->set_value("position_y", int(instance->get_offset(Y)));
    m_og->set_value("position_z", int(instance->get_offset(Z)));
}

void ObjectManipulation::update_position_value(const Vec3d& position)
{
    m_og->set_value("position_x", int(position(0)));
    m_og->set_value("position_y", int(position(1)));
    m_og->set_value("position_z", int(position(2)));
}

void ObjectManipulation::update_scale_value(const Vec3d& scaling_factor)
{
    // this is temporary
    // to be able to update the values as size
    // we need to store somewhere the original size
    // or have it passed as parameter
    if (!m_is_percent_scale)
        m_og->set_value("scale_unit", _("%"));

    auto scale = scaling_factor * 100.0;
    m_og->set_value("scale_x", int(scale(0)));
    m_og->set_value("scale_y", int(scale(1)));
    m_og->set_value("scale_z", int(scale(2)));
}

void ObjectManipulation::update_rotation_values()
{
    update_rotation_value(wxGetApp().mainframe->m_plater->model().objects[ol_selection()]->instances.front()->get_rotation());
}

void ObjectManipulation::update_rotation_value(double angle, Axis axis)
{
    std::string axis_str;
    switch (axis) {
    case X: {
        axis_str = "rotation_x";
        break; }
    case Y: {
        axis_str = "rotation_y";
        break; }
    case Z: {
        axis_str = "rotation_z";
        break; }
    }

    m_og->set_value(axis_str, round_nearest(int(Geometry::rad2deg(angle)), 0));
}

void ObjectManipulation::update_rotation_value(const Vec3d& rotation)
{
    m_og->set_value("rotation_x", int(round_nearest(Geometry::rad2deg(rotation(0)), 0)));
    m_og->set_value("rotation_y", int(round_nearest(Geometry::rad2deg(rotation(1)), 0)));
    m_og->set_value("rotation_z", int(round_nearest(Geometry::rad2deg(rotation(2)), 0)));
}

void ObjectManipulation::show_object_name(bool show)
{
    wxGridSizer* grid_sizer = m_og->get_grid_sizer();
    grid_sizer->Show(static_cast<size_t>(0), show);
    grid_sizer->Show(static_cast<size_t>(1), show);
}


} //namespace GUI
} //namespace Slic3r 