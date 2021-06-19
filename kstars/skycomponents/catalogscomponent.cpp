/***************************************************************************
                catalogscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Jun 2021
    copyright            : (C) 2021 by Valentin Boettcher, Jason Harris
    email                : hiro at protagon.space; @hiro98:tchncs.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <cmath>
#include "catalogscomponent.h"
#include "skypainter.h"
#include "skymap.h"
#include "kstarsdata.h"
#include "Options.h"
#include "MeshIterator.h"
#include "projections/projector.h"
#include "skylabeler.h"
#include "kstars_debug.h"
#include "kstars.h"
#include "skymapcomposite.h"
#include "kspaths.h"

CatalogsComponent::CatalogsComponent(SkyComposite *parent, const QString &db_filename,
                                     bool load_default)
    : SkyComponent(parent), m_db_manager(db_filename), m_skyMesh{ SkyMesh::Create(
                                                           m_db_manager.htmesh_level()) },
      m_cache(m_skyMesh->size(), calculateCacheSize(Options::dSOCachePercentage()))
{
    if (load_default)
    {
        const auto &default_file = KSPaths::locate(QStandardPaths::GenericDataLocation,
                                                   Options::dSODefaultCatalogFilename());

        if (QFile(default_file).exists())
        {
            m_db_manager.import_catalog(default_file, false);
        }
    }

    loadStaticObjects();
    qCInfo(KSTARS) << "Loaded DSO catalogs.";
}

std::pair<double, double> compute_maglim()
{
    double maglim = Options::magLimitDrawDeepSky();

    //adjust maglimit for ZoomLevel
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz   = log10(Options::zoomFactor());
    if (lgz <= 0.75 * lgmax)
        maglim -=
            (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut()) *
            (0.75 * lgmax - lgz) / (0.75 * lgmax - lgmin);

    double label_maglim = maglim * (Options::deepSkyLabelDensity() / 100);
    label_maglim        = std::min(label_maglim, maglim);

    return { maglim, label_maglim };
}

void CatalogsComponent::draw(SkyPainter *skyp)
{
    if (!selected() || Options::zoomFactor() < Options::dSOMinZoomFactor())
        return;

    KStarsData *data          = KStarsData::Instance();
    const auto &default_color = data->colorScheme()->colorNamed("DSOColor");
    skyp->setPen(default_color);
    skyp->setBrush(Qt::NoBrush);

    bool showUnknownMagObjects = Options::showUnknownMagObjects();
    double maglim, labelMaglim;
    std::tie(maglim, labelMaglim) = compute_maglim();

    auto &labeler = *SkyLabeler::Instance();
    labeler.setPen(
        QColor(KStarsData::Instance()->colorScheme()->colorNamed("DSNameColor")));

    auto &map       = *SkyMap::Instance();
    auto hideLabels = (map.isSlewing() && Options::hideOnSlew()) ||
                      !(Options::showDeepSkyMagnitudes() || Options::showDeepSkyNames());

    auto &proj = *map.projector();

    updateSkyMesh(map);
    MeshIterator region(m_skyMesh, DRAW_BUF);

    size_t num_trixels{ 0 };
    while (region.hasNext())
    {
        num_trixels++;
        Trixel trixel = region.next();

        auto &objects = m_cache[trixel];
        if (!objects.is_set())
        {
            try
            {
                objects = m_db_manager.get_objects_in_trixel(trixel);
            }
            catch (const CatalogsDB::DatabaseError &e)
            {
                qCCritical(KSTARS)
                    << "Could not load catlog objects in trixel: " << trixel << ", "
                    << e.what();
                throw; // do not silently fail
            }
        }

        for (auto &object : objects.data())
        {
            auto mag          = object.mag();
            bool mag_unknown  = std::isnan(mag) || (mag > 36.0);
            bool magCriterion = (mag_unknown && showUnknownMagObjects) || (mag < maglim);

            if (!magCriterion && !mag_unknown)
                break; // the objects are strictly sorted by magnitude
                       // unknown magnitude first
            if (!magCriterion)
                continue;

            float size = object.a() * dms::PI * Options::zoomFactor() / 10800.0;

            bool sizeCriterion =
                (size > 1.0 || size == 0 || Options::zoomFactor() > 2000.);

            if (sizeCriterion)
            {
                object.JITupdate();
                auto &color = m_catalog_colors[object.catalogId()];
                if (!color.isValid())
                {
                    const auto &catalog_color = object.getCatalog().color;
                    if (catalog_color == "")
                        color = default_color;
                    else
                        color = QColor(catalog_color);
                }

                skyp->setPen(color);
                bool drawn = skyp->drawCatalogObject(object);

                if (drawn && !(hideLabels || mag > labelMaglim))
                    labeler.drawNameLabel(&object, proj.toScreen(&object));
            }
        }
    }

    // prune only if the to-be-pruned trixels are likely not visible
    // and we are not zooming
    m_cache.prune(num_trixels * 1.2);
};

void CatalogsComponent::updateSkyMesh(SkyMap &map, MeshBufNum_t buf)
{
    SkyPoint *focus = map.focus();
    float radius    = map.projector()->fov();
    if (radius > 180.0)
        radius = 180.0;

    m_skyMesh->aperture(focus, radius + 1.0, buf);
}

CatalogObject &CatalogsComponent::insertStaticObject(CatalogObject obj)
{
    auto trixel     = m_skyMesh->index(&obj);
    auto &lst       = m_static_objects[trixel];
    auto found_iter = std::find(lst.begin(), lst.end(), obj);

    // Ideally, we would remove the object from ObjectsList if it's
    // respective catalog is disabled, but there ain't a good way to
    // do this right now
    if (!(found_iter == lst.end()))
    {
        auto &found = *found_iter;
        found       = obj;
        found.JITupdate();
        return found;
    }

    obj.JITupdate();

    lst.push_back(std::move(obj));
    auto &inserted = lst.back();

    // we don't bother with translations here
    auto &object_list = objectLists(inserted.type());
    object_list.append({ inserted.name(), &inserted });
    object_list.append({ inserted.longname(), &inserted });

    return inserted;
}

SkyObject *CatalogsComponent::findByName(const QString &name)
{
    auto objects = m_db_manager.find_objects_by_name(name, 1);

    if (objects.size() == 0)
        return nullptr;

    return &insertStaticObject(objects.front());
}

void CatalogsComponent::loadStaticObjects()
{
    auto objects = m_db_manager.get_objects(Options::magLimitDrawDeepSky(),
                                            Options::numberStaticObjects());
    for (auto &object : objects)
        insertStaticObject(object);
}

void CatalogsComponent::objectsInArea(QList<SkyObject *> &list, const SkyRegion &region)
{
    if (!selected())
        return;

    for (SkyRegion::const_iterator it = region.constBegin(); it != region.constEnd();
         ++it)
    {
        try
        {
            for (auto &dso : m_db_manager.get_objects_in_trixel(it.key()))
            {
                auto &obj = insertStaticObject(dso);
                list.append(&obj);
            }
        }
        catch (const CatalogsDB::DatabaseError &e)
        {
            qCCritical(KSTARS) << "Could not load catlog objects in trixel: " << it.key()
                               << ", " << e.what();
            throw; // do not silently fail
        }
    }
}

SkyObject *CatalogsComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;

    m_skyMesh->aperture(p, maxrad, OBJ_NEAREST_BUF);
    MeshIterator region(m_skyMesh, OBJ_NEAREST_BUF);
    double smallest_r{ 360 };
    CatalogObject nearest{};
    bool found{ false };

    while (region.hasNext())
    {
        auto trixel = region.next();
        try
        {
            auto objects = m_db_manager.get_objects_in_trixel(trixel);
            if (!found)
                found = objects.size() > 0;

            for (auto &obj : objects)
            {
                obj.JITupdate();

                double r = obj.angularDistanceTo(p).Degrees();
                if (r < smallest_r)
                {
                    smallest_r = r;
                    nearest    = obj;
                }
            }
        }
        catch (const CatalogsDB::DatabaseError &e)
        {
            KMessageBox::detailedError(
                nullptr, i18n("Could not load catlog objects in trixel: %1", trixel),
                e.what());
            throw; // do not silently fail
        }
    }

    if (!found)
        return nullptr;

    maxrad = smallest_r;

    return &insertStaticObject(nearest);
}
