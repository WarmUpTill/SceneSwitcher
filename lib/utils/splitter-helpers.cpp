#include "splitter-helpers.hpp"

#include <QSplitter>

namespace advss {

void SaveSplitterPos(const QList<int> &sizes, obs_data_t *obj,
		     const std::string &name)
{
	auto array = obs_data_array_create();
	for (int i = 0; i < sizes.count(); ++i) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_int(array_obj, "pos", sizes[i]);
		obs_data_array_push_back(array, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, name.c_str(), array);
	obs_data_array_release(array);
}

void LoadSplitterPos(QList<int> &sizes, obs_data_t *obj,
		     const std::string &name)
{
	sizes.clear();
	obs_data_array_t *array = obs_data_get_array(obj, name.c_str());
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		sizes << obs_data_get_int(item, "pos");
		obs_data_release(item);
	}
	obs_data_array_release(array);
}

void CenterSplitterPosition(QSplitter *splitter)
{
	splitter->setSizes(QList<int>() << 999999 << 999999);
}

void MaximizeFirstSplitterEntry(QSplitter *splitter)
{
	QList<int> newSizes;
	newSizes << 999999;
	for (int i = 0; i < splitter->sizes().size() - 1; i++) {
		newSizes << 0;
	}
	splitter->setSizes(newSizes);
}

void ReduceSizeOfSplitterIdx(QSplitter *splitter, int idx)
{
	auto sizes = splitter->sizes();
	int sum = sizes[0] + sizes[1];
	int reducedSize = sum / 10;
	sizes[idx] = reducedSize;
	sizes[(idx + 1) % 2] = sum - reducedSize;
	splitter->setSizes(sizes);
}

} // namespace advss
