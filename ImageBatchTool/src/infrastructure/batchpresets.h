#pragma once
#include "domain/batchoptions.h"
#include <QStringList>
namespace BatchPresets {
QStringList names();
BatchProcessing::Parameters load(const QString &name);
void save(const QString &name,const BatchProcessing::Parameters &parameters);
void remove(const QString &name);
}
