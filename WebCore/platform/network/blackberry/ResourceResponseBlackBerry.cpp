#include "config.h"
#include "ResourceResponse.h"
#include "NotImplemented.h"

namespace WebCore {

PassOwnPtr<CrossThreadResourceResponseData> ResourceResponse::doPlatformCopyData(PassOwnPtr<CrossThreadResourceResponseData> data) const
{
    data->m_isWML = m_isWML;
    return data;
}

void ResourceResponse::doPlatformAdopt(PassOwnPtr<CrossThreadResourceResponseData> data)
{
    m_isWML = data->m_isWML;
}

} // namespace WebCore
