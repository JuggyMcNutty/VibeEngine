
#include "Precomp.h"
#include "UField.h"

void UField::Load(ObjectStream* stream)
{
	GC::CodeChanged();
	UObject::Load(stream);
	BaseField = stream->ReadObject<UField>();
	Next = stream->ReadObject<UField>();
}

void UField::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);
	stream->WriteObject(BaseField);
	stream->WriteObject(Next);
}

void UField::Mark(GCMarker& marker)
{
	UObject::Mark(marker);
	marker.SetField("BaseField");
	marker.MarkCode(BaseField);
	marker.SetField("Next");
	marker.MarkCode(Next);
}
