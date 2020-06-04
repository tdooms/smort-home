//============================================================================
// @author      : Thomas Dooms
// @date        : 6/1/20
// @copyright   : BA2 Informatica - Thomas Dooms - University of Antwerp
//============================================================================


#pragma once

#include <QtCore/QAbstractListModel>


class YeelightModel final : public QAbstractListModel
{
    public:
    [[nodiscard]] int rowCount(const QModelIndex& parent) const final
    {
        return modelData.size();
    }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const final
    {
        return modelData[index.row()];
    }

    private:
    std::vector<uint32_t> modelData;
};