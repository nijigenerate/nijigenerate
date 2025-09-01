/**
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen


    Here you will find easy access for some configurable things you can set for nijigenerate builds
    These are mostly useful for unofficial builds. Make sure to update this file for unofficial builds
    as otherwise we'll end up getting support requests for distributions we don't maintain.

    -- NOTE --
    SHOULD indicates that a line should be changed if the condition given is met.
*/
module nijigenerate.config;

/**
    Name of the artist for the included banner.

    If you change the banner you SHOULD change this.
*/
enum INC_BANNER_ARTIST_NAME = "seagetch";

/**
    Link to the artist's preferred social media,
    or art posting page.
*/
enum INC_BANNER_ARTIST_PAGE = "https://twitter.com/seagetch";

/**
    URI for bug reports, for unofficial builds this SHOULD be changed.
*/
enum INC_BUG_REPORT_URI = "https://github.com/nijigenerate/nijigenerate/issues/new?assignees=&labels=bug&template=bug-report.yml&title=%5BBUG%5D";

/**
    URI for feature requests, for the most part this doesn't need to be changed
    unless you're making a fork.
*/
enum INC_FEATURE_REQ_URI = "https://github.com/nijigenerate/nijigenerate/issues/new?assignees=&labels=enhancement&template=feature_request.yml&title=%5BFeature+Request%5D";

/**
    URI to website for the project, change this if you fork the project.
*/
enum INC_INFO_WEBSITE_URI = "https://github.com/nijigenerate/";

/**
    URI to documentation provided for this software, by default it'll direct to the nijilive Documentation

    If you modify the software by adding new features it's highly recommended that you change this link to
    documentation which fits with the new featureset.
*/
enum INC_INFO_DOCS_URI = "https://docs.github.com/nijigenerate/";

/**
    URI to the discord server where support is provided.

    Set to "" to disable the link.
*/
enum INC_INFO_DISCORD_URI = "";

/**
    Whether to show donation links.

    These are hardcoded, if you're forking the project set this to false.
*/
enum INC_INFO_SHOW_DONATE_LINKS = true;
