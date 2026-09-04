#include "OwlLogo.h"

namespace {

static const char kOwlSvg[] = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1254 1254" width="1254" height="1254"><path fill="#F2AB21" fill-rule="evenodd" d="M 345 932 L 345 975 L 627 1182 L 634 1179 L 910 975 L 910 932 L 908 932 L 630 1138 L 627 1138 L 351 934 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 345 836 L 345 892 L 612 1092 L 618 1094 L 618 1034 L 388 865 L 352 839 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 345 731 L 345 795 L 613 993 L 618 995 L 618 928 L 468 816 L 348 731 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 620 625 L 604 631 L 594 641 L 586 662 L 585 682 L 591 705 L 602 732 L 626 780 L 630 781 L 655 733 L 663 714 L 672 684 L 672 665 L 668 650 L 663 640 L 655 632 L 637 625 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 283 577 L 283 597 L 286 601 L 296 605 L 317 674 L 324 684 L 330 689 L 340 694 L 349 696 L 524 695 L 538 689 L 549 677 L 552 672 L 570 617 L 576 613 L 599 603 L 624 599 L 650 601 L 672 607 L 670 583 L 658 579 L 630 575 L 600 578 L 576 587 L 572 585 L 571 581 L 565 575 L 555 571 L 291 571 L 286 573 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 1012 518 L 1008 518 L 1008 760 L 934 822 L 934 867 L 938 866 L 1043 776 L 1043 558 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 244 517 L 211 558 L 211 776 L 306 857 L 320 867 L 320 819 L 245 757 L 246 517 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 287 349 L 277 374 L 272 399 L 271 425 L 275 453 L 281 473 L 294 499 L 304 512 L 324 529 L 349 540 L 370 544 L 422 545 L 443 543 L 465 537 L 482 529 L 496 519 L 509 506 L 518 492 L 518 488 L 469 452 L 426 424 L 423 424 L 423 434 L 418 448 L 403 463 L 391 468 L 375 468 L 362 463 L 348 449 L 343 437 L 342 424 L 348 406 L 359 394 L 365 391 L 365 389 L 318 363 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 512 295 L 534 314 L 540 315 L 626 261 L 634 263 L 717 315 L 723 313 L 745 295 L 745 292 L 627 218 L 513 292 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 1077 120 L 1073 121 L 1005 165 L 885 246 L 823 291 L 764 340 L 720 385 L 686 431 L 661 479 L 650 510 L 644 536 L 646 536 L 683 499 L 707 478 L 774 427 L 833 388 L 1022 273 L 1064 160 Z"/><path fill="#F2AB21" fill-rule="evenodd" d="M 178 120 L 236 274 L 419 385 L 474 421 L 543 473 L 575 501 L 611 537 L 613 537 L 609 516 L 601 491 L 594 474 L 570 429 L 538 386 L 493 340 L 430 288 L 362 239 L 221 145 L 184 122 Z"/><path fill="#78DE10" fill-rule="evenodd" d="M 910 836 L 906 837 L 639 1033 L 638 1095 L 645 1092 L 910 893 Z"/><path fill="#78DE10" fill-rule="evenodd" d="M 910 730 L 905 732 L 640 926 L 638 928 L 638 995 L 641 995 L 907 799 L 910 796 Z"/><path fill="#78DE10" fill-rule="evenodd" d="M 671 585 L 674 610 L 688 618 L 704 669 L 709 679 L 722 691 L 737 696 L 907 696 L 918 693 L 928 687 L 937 676 L 961 603 L 968 602 L 973 596 L 973 577 L 966 571 L 701 571 L 689 577 L 684 586 L 680 587 Z"/><path fill="#78DE10" fill-rule="evenodd" d="M 968 349 L 965 349 L 890 388 L 890 392 L 893 393 L 905 406 L 911 423 L 911 433 L 907 446 L 891 463 L 877 468 L 864 468 L 850 463 L 836 449 L 832 441 L 830 425 L 828 425 L 763 469 L 740 488 L 741 492 L 750 505 L 769 523 L 789 535 L 812 543 L 829 545 L 885 544 L 909 538 L 925 530 L 935 523 L 950 508 L 961 492 L 971 471 L 978 446 L 981 421 L 980 394 L 976 372 Z"/></svg>)svg";

static lv_image_dsc_t kOwlSvgDescriptor = {};

void initializeSvgDescriptor()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }

    kOwlSvgDescriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    kOwlSvgDescriptor.header.w = 1254;
    kOwlSvgDescriptor.header.h = 1254;
    kOwlSvgDescriptor.data_size = sizeof(kOwlSvg) - 1;
    kOwlSvgDescriptor.data = reinterpret_cast<const uint8_t *>(kOwlSvg);

    initialized = true;
}

} // namespace

void OwlLogo::create(lv_obj_t *parent, int x, int y, int width, int height)
{
    initializeSvgDescriptor();

    _image = lv_image_create(parent);
    lv_image_set_src(_image, &kOwlSvgDescriptor);
    lv_obj_set_pos(_image, x, y);
    lv_obj_set_size(_image, width, height);
    lv_image_set_inner_align(_image, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_antialias(_image, true);
}

void OwlLogo::setHidden(bool hidden)
{
    if (_image == nullptr) return;
    if (hidden) lv_obj_add_flag(_image, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(_image, LV_OBJ_FLAG_HIDDEN);
}
