// SPDX-License-Identifier: GPL-2.0

#include "command_pictures.h"
#include "core/errorhelper.h"
#include "core/subsurface-qt/divelistnotifier.h"
#include "qt-models/divelocationmodel.h"

namespace Command {

static picture *dive_get_picture(const dive *d, const QString &fn)
{
	int idx = get_picture_idx(&d->pictures, qPrintable(fn));
	return idx < 0 ? nullptr : &d->pictures.pictures[idx];
}

SetPictureOffset::SetPictureOffset(dive *dIn, const QString &filenameIn, offset_t offsetIn) :
	d(dIn), filename(filenameIn), offset(offsetIn)
{
	if (!dive_get_picture(d, filename))
		d = nullptr;
	setText(Command::Base::tr("Change media time"));
}

void SetPictureOffset::redo()
{
	picture *pic = dive_get_picture(d, filename);
	if (!pic) {
		report_info("SetPictureOffset::redo(): picture disappeared!");
		return;
	}
	std::swap(pic->offset, offset);
	offset_t newOffset = pic->offset;

	// Instead of trying to be smart, let's simply resort the picture table.
	// If someone complains about speed, do our usual "smart" thing.
	sort_picture_table(&d->pictures);
	emit diveListNotifier.pictureOffsetChanged(d, filename, newOffset);
	invalidate_dive_cache(d);
}

// Undo and redo do the same thing
void SetPictureOffset::undo()
{
	redo();
}

bool SetPictureOffset::workToBeDone()
{
	return !!d;
}

// Filter out pictures that don't exist and sort them according to the backend
static PictureListForDeletion filterPictureListForDeletion(const PictureListForDeletion &p)
{
	PictureListForDeletion res;
	res.d = p.d;
	res.filenames.reserve(p.filenames.size());
	for (int i = 0; i < p.d->pictures.nr; ++i) {
		std::string fn = p.d->pictures.pictures[i].filename;
		if (std::find(p.filenames.begin(), p.filenames.end(), fn) != p.filenames.end())
			res.filenames.push_back(fn);
	}
	return res;
}

// Helper function to remove pictures. Clears the passed-in vector.
static std::vector<PictureListForAddition> removePictures(std::vector<PictureListForDeletion> &picturesToRemove)
{
	std::vector<PictureListForAddition> res;
	for (const PictureListForDeletion &list: picturesToRemove) {
		QVector<QString> filenames;
		PictureListForAddition toAdd;
		toAdd.d = list.d;
		for (const std::string &fn: list.filenames) {
			int idx = get_picture_idx(&list.d->pictures, fn.c_str());
			if (idx < 0) {
				report_info("removePictures(): picture disappeared!");
				continue; // Huh? We made sure that this can't happen by filtering out non-existent pictures.
			}
			filenames.push_back(QString::fromStdString(fn));
			toAdd.pics.emplace_back(list.d->pictures.pictures[idx]);
			remove_from_picture_table(&list.d->pictures, idx);
		}
		if (!toAdd.pics.empty())
			res.push_back(toAdd);
		invalidate_dive_cache(list.d);
		emit diveListNotifier.picturesRemoved(list.d, std::move(filenames));
	}
	picturesToRemove.clear();
	return res;
}

// Helper function to add pictures. Clears the passed-in vector.
static std::vector<PictureListForDeletion> addPictures(std::vector<PictureListForAddition> &picturesToAdd)
{
	// We play it extra safe here and again filter out those pictures that
	// are already added to the dive. There should be no way for this to
	// happen, as we checked that before.
	std::vector<PictureListForDeletion> res;
	for (const PictureListForAddition &list: picturesToAdd) {
		QVector<PictureObj> picsForSignal;
		PictureListForDeletion toRemove;
		toRemove.d = list.d;
		for (const PictureObj &pic: list.pics) {
			int idx = get_picture_idx(&list.d->pictures, pic.filename.c_str()); // This should *not* already exist!
			if (idx >= 0) {
				report_info("addPictures(): picture disappeared!");
				continue; // Huh? We made sure that this can't happen by filtering out existing pictures.
			}
			picsForSignal.push_back(pic);
			add_picture(&list.d->pictures, pic.toCore());
			toRemove.filenames.push_back(pic.filename);
		}
		if (!toRemove.filenames.empty())
			res.push_back(toRemove);
		invalidate_dive_cache(list.d);
		emit diveListNotifier.picturesAdded(list.d, std::move(picsForSignal));
	}
	picturesToAdd.clear();
	return res;
}

RemovePictures::RemovePictures(const std::vector<PictureListForDeletion> &pictures)
{
	// Filter out the pictures that don't actually exist. In principle this shouldn't be necessary.
	// Nevertheless, let's play it save. This has the additional benefit of sorting the pictures
	// according to the backend if, for some reason, the caller passed the pictures in in random order.
	picturesToRemove.reserve(pictures.size());
	size_t count = 0;
	for (const PictureListForDeletion &p: pictures) {
		PictureListForDeletion filtered = filterPictureListForDeletion(p);
		if (filtered.filenames.size())
			picturesToRemove.push_back(p);
		count += filtered.filenames.size();
	}
	if (count == 0) {
		picturesToRemove.clear(); // This signals that nothing is to be done
		return;
	}
	setText(Command::Base::tr("remove %n pictures(s)", "", count));
}

bool RemovePictures::workToBeDone()
{
	return !picturesToRemove.empty();
}

void RemovePictures::undo()
{
	picturesToRemove = addPictures(picturesToAdd);
}

void RemovePictures::redo()
{
	picturesToAdd = removePictures(picturesToRemove);
}

AddPictures::AddPictures(const std::vector<PictureListForAddition> &pictures) : picturesToAdd(pictures)
{
	// Sort the pictures according to the backend-rules. Moreover see if we have to set / create divesites.
	size_t count = 0;
	for (PictureListForAddition &p: picturesToAdd) {
		count += p.pics.size();
		std::sort(p.pics.begin(), p.pics.end());

		// Find a picture with a location
		auto it = std::find_if(p.pics.begin(), p.pics.end(), [](const PictureObj &p) { return has_location(&p.location); });
		if (it != p.pics.end()) {
			// There is a dive with a location, we might want to modify the dive accordingly.
			struct dive_site *ds = p.d->dive_site;
			if (!ds) {
				// This dive doesn't yet have a dive site -> add a new dive site.
				QString name = Command::Base::tr("unnamed dive site");
				sitesToAdd.push_back(std::make_unique<dive_site>(qPrintable(name), &it->location));
				sitesToSet.push_back({ p.d, sitesToAdd.back().get() });
			} else if (!dive_site_has_gps_location(ds)) {
				// This dive has a dive site, but without coordinates. Let's add them.
				sitesToEdit.push_back({ ds, it->location });
			}
		}
	}

	if (count == 0) {
		picturesToAdd.clear(); // This signals that nothing is to be done
		return;
	}
	setText(Command::Base::tr("add %n pictures(s)", "", count));
}

bool AddPictures::workToBeDone()
{
	return !picturesToAdd.empty();
}

void AddPictures::swapDiveSites()
{
	for (DiveSiteEntry &entry: sitesToSet) {
		dive_site *ds = entry.d->dive_site;
		if (ds)
			unregister_dive_from_dive_site(entry.d); // the dive-site pointer in the dive is now NULL
		std::swap(ds, entry.ds);
		if (ds)
			ds->add_dive(entry.d);
		emit diveListNotifier.divesChanged(QVector<dive *>{ entry.d }, DiveField::DIVESITE);
	}

	for (DiveSiteEditEntry &entry: sitesToEdit) {
		std::swap(entry.ds->location, entry.location);
		emit diveListNotifier.diveSiteChanged(entry.ds, LocationInformationModel::LOCATION); // Inform frontend of changed dive site.
	}
}

void AddPictures::undo()
{
	swapDiveSites();
	picturesToAdd = removePictures(picturesToRemove);

	// Remove dive sites
	for (dive_site *siteToRemove: sitesToRemove) {
		auto res = divelog.sites->pull(siteToRemove);
		sitesToAdd.push_back(std::move(res.ptr));
		emit diveListNotifier.diveSiteDeleted(siteToRemove, res.idx); // Inform frontend of removed dive site.
	}
	sitesToRemove.clear();
}

void AddPictures::redo()
{
	// Add dive sites
	for (std::unique_ptr<dive_site> &siteToAdd: sitesToAdd) {
		auto res = divelog.sites->register_site(std::move(siteToAdd)); // Return ownership to backend.
		sitesToRemove.push_back(res.ptr);
		emit diveListNotifier.diveSiteAdded(sitesToRemove.back(), res.idx); // Inform frontend of new dive site.
	}
	sitesToAdd.clear();

	swapDiveSites();
	picturesToRemove = addPictures(picturesToAdd);
}

} // namespace Command
