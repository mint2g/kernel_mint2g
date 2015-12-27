/*
 * speaker-pa.c
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <sound/audio_pa.h>

paudio_pa_control audio_pa_amplifier;
EXPORT_SYMBOL_GPL(audio_pa_amplifier);

static __devinit int speaker_pa_probe(struct platform_device *pdev)
{
	audio_pa_amplifier = platform_get_drvdata(pdev);
	if (audio_pa_amplifier) {
		if (audio_pa_amplifier->speaker.init)
			audio_pa_amplifier->speaker.init();
		if (audio_pa_amplifier->earpiece.init)
			audio_pa_amplifier->earpiece.init();
		if (audio_pa_amplifier->headset.init)
			audio_pa_amplifier->headset.init();
	}
	return 0;
}

static int __devexit speaker_pa_remove(struct platform_device *pdev)
{
	audio_pa_amplifier = NULL;
	return 0;
}

static struct platform_driver speaker_pa_driver = {
	.driver = {
		.name = "speaker-pa",
		.owner = THIS_MODULE,
	},
	.probe = speaker_pa_probe,
	.remove = __devexit_p(speaker_pa_remove),
};

static int __init speaker_pa_init(void)
{
	return platform_driver_register(&speaker_pa_driver);
}

arch_initcall(speaker_pa_init);

MODULE_DESCRIPTION("ALSA SoC Speaker PA Control");
MODULE_AUTHOR("Luther Ge <luther.ge@spreadtrum.com>");
MODULE_LICENSE("GPL");
