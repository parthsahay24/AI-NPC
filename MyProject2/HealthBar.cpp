// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthBar.h"
#include "Components/ProgressBar.h"
#include "HealthBarUW1.h"
void UHealthBar::SetPercent(float percent)
{
    HealthBarUW = Cast<UHealthBarUW1>(GetUserWidgetObject());
    if (HealthBarUW && HealthBarUW->HealthBar)
    {
        HealthBarUW->HealthBar->SetPercent(percent);
    }
}
